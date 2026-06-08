#include "github_update_checker.h"

#include "github_release_parser.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace {

QUrl makeGitHubReleasesApiUrl(const QString& owner, const QString& repo)
{
    return QUrl(
        QStringLiteral("https://api.github.com/repos/%1/%2/releases?per_page=100")
            .arg(owner, repo)
    );
}

QUrl makeGitHubReleasePageUrl(
    const QString& owner,
    const QString& repo,
    const GitHubReleaseInfo& release)
{
    if (!release.htmlUrl.trimmed().isEmpty())
        return QUrl(release.htmlUrl);

    return QUrl(
        QStringLiteral("https://github.com/%1/%2/releases/tag/%3")
            .arg(owner, repo, release.tag)
    );
}

QString effectiveCurrentVersion(const QString& version)
{
    const QString trimmed = version.trimmed();

    if (!trimmed.isEmpty())
        return trimmed;

    return QStringLiteral("unknown");
}

} // namespace

GitHubUpdateCheckResult checkGitHubForUpdates(
    const GitHubUpdateCheckOptions& options)
{
    GitHubUpdateCheckResult result;

    result.owner = options.owner;
    result.repo = options.repo;
    result.currentVersion = effectiveCurrentVersion(options.currentVersion);
    result.apiUrl = makeGitHubReleasesApiUrl(options.owner, options.repo);

    QNetworkAccessManager manager;

    QNetworkRequest request(result.apiUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, options.userAgent);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

    QNetworkReply* reply = manager.get(request);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    bool timedOut = false;

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;

        if (reply)
            reply->abort();

        loop.quit();
    });

    timeout.start(options.timeoutMilliseconds);
    loop.exec();

    if (timeout.isActive())
        timeout.stop();

    result.httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorString = reply->errorString();
    const QByteArray data = reply->readAll();

    reply->deleteLater();

    if (timedOut) {
        result.errorMessage = QStringLiteral("GitHub request timed out.");
        return result;
    }

    if (networkError != QNetworkReply::NoError) {
        result.errorMessage =
            QStringLiteral("GitHub request failed: %1").arg(networkErrorString);
        return result;
    }

    if (result.httpStatus < 200 || result.httpStatus >= 300) {
        result.errorMessage =
            QStringLiteral("GitHub request failed with HTTP status %1.")
                .arg(result.httpStatus);
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        result.errorMessage =
            QStringLiteral("GitHub releases response could not be parsed.");
        return result;
    }

    const QJsonArray releases = document.array();

    result.latestRelease =
        latestReleaseFromJson(releases, options.includePrereleases);

    if (!result.latestRelease.isValid()) {
        result.errorMessage =
            QStringLiteral("No valid GitHub release was found for %1/%2.")
                .arg(options.owner, options.repo);
        return result;
    }

    result.latestReleaseUrl =
        makeGitHubReleasePageUrl(options.owner, options.repo, result.latestRelease);

    result.currentRelease =
        releaseForCurrentVersionFromJson(
            releases,
            result.currentVersion,
            options.includePrereleases
        );

    if (result.currentRelease.isValid()) {
        result.currentReleaseDate = result.currentRelease.publishedAt;
        result.currentReleaseDateFromApi = true;
    } else {
        result.currentReleaseDate = options.fallbackCurrentReleaseDate;
        result.currentReleaseDateFromApi = false;
    }

    if (!result.currentReleaseDate.isValid()) {
        result.errorMessage =
            QStringLiteral(
                "Current release date is unknown. Set release metadata or tag the current version on GitHub."
            );
        return result;
    }

    bool versionComparisonOk = false;
    const int versionComparison = compareReleaseTags(
        result.latestRelease.tag,
        result.currentVersion,
        &versionComparisonOk
    );

    if (versionComparisonOk) {
        result.updateAvailable = versionComparison > 0;
    } else {
        const QDate currentReleaseDay = result.currentReleaseDate.toUTC().date();
        const QDate latestReleaseDay = result.latestRelease.publishedAt.toUTC().date();

        result.updateAvailable =
            currentReleaseDay.isValid() &&
            latestReleaseDay.isValid() &&
            latestReleaseDay > currentReleaseDay;
    }

    result.ok = true;
    return result;
}
