#include "github_release_parser.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1Char>
#include <QString>

namespace {

    QString normalisedVersionTag(QString value)
    {
        value = value.trimmed();

        if (value.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
            value.remove(0, 1);

        return value;
    }

    GitHubReleaseInfo releaseInfoFromJsonObject(const QJsonObject &release)
    {
        GitHubReleaseInfo info;

        info.tag = release.value(QStringLiteral("tag_name")).toString();
        info.prerelease = release.value(QStringLiteral("prerelease")).toBool(false);
        info.htmlUrl = release.value(QStringLiteral("html_url")).toString();

        const QString publishedAtString =
        release.value(QStringLiteral("published_at")).toString();

        info.publishedAt =
        QDateTime::fromString(publishedAtString, Qt::ISODate);

        return info;
    }

} // namespace

bool releaseTagMatchesCurrentVersion(
    const QString &releaseTag,
    const QString &currentVersion
)
{
    const QString tag = normalisedVersionTag(releaseTag);
    const QString version = normalisedVersionTag(currentVersion);

    if (tag.isEmpty() || version.isEmpty())
        return false;

    return tag.compare(version, Qt::CaseInsensitive) == 0;
}

GitHubReleaseInfo latestReleaseFromJson(
    const QJsonArray &releases,
    bool includePrereleases
)
{
    GitHubReleaseInfo latest;

    for (const QJsonValue &value : releases) {
        if (!value.isObject())
            continue;

        const QJsonObject release = value.toObject();
        const GitHubReleaseInfo candidate =
        releaseInfoFromJsonObject(release);

        if (!candidate.isValid())
            continue;

        if (!includePrereleases && candidate.prerelease) {
            qDebug() << "[GitHub] Skipping prerelease because stable channel is selected:"
            << candidate.tag;
            continue;
        }

        qDebug() << "[GitHub] Candidate:"
        << "tag:" << candidate.tag
        << "prerelease:" << candidate.prerelease
        << "published_at:" << candidate.publishedAt.toString(Qt::ISODate)
        << "url:" << candidate.htmlUrl;

        if (!latest.isValid() || candidate.publishedAt > latest.publishedAt)
            latest = candidate;
    }

    return latest;
}

GitHubReleaseInfo releaseForCurrentVersionFromJson(
    const QJsonArray &releases,
    const QString &currentVersion,
    bool includePrereleases
)
{
    for (const QJsonValue &value : releases) {
        if (!value.isObject())
            continue;

        const QJsonObject release = value.toObject();
        const GitHubReleaseInfo candidate =
        releaseInfoFromJsonObject(release);

        if (!candidate.isValid())
            continue;

        if (!releaseTagMatchesCurrentVersion(candidate.tag, currentVersion))
            continue;

        if (!includePrereleases && candidate.prerelease) {
            qDebug() << "[GitHub] Skipping current-version prerelease because stable channel is selected:"
            << candidate.tag;
            continue;
        }

        qDebug() << "[GitHub] Current version release found:"
        << "tag:" << candidate.tag
        << "prerelease:" << candidate.prerelease
        << "published_at:" << candidate.publishedAt.toString(Qt::ISODate);

        return candidate;
    }

    return {};
}
