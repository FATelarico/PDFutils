#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>

void MainWindow::loadReleaseChannelPreference()
{
    QSettings settings;

    WantsPrerel = settings.value(
                              "updates/wantsPrereleases",
                              false
                              ).toBool();

    updateReleaseChannelActionText();
}

void MainWindow::saveReleaseChannelPreference() const
{
    QSettings settings;

    settings.setValue(
        "updates/wantsPrereleases",
        WantsPrerel
        );
}

void MainWindow::updateReleaseChannelActionText()
{
    ui->link00c->setChecked(WantsPrerel);

    /*
        ui->link00c->setText(
            WantsPrerel ? "Unstable channel" : "Stable channel"
        );
    */
}


bool MainWindow::releaseTagMatchesCurrentVersion(
    const QString &releaseTag,
    const QString &currentVersion
    ) const
{
    QString tag = releaseTag.trimmed();
    QString version = currentVersion.trimmed();

    if (tag.isEmpty() || version.isEmpty())
        return false;

    if (tag.compare(version, Qt::CaseInsensitive) == 0)
        return true;

    if (tag.startsWith('v', Qt::CaseInsensitive) &&
        tag.mid(1).compare(version, Qt::CaseInsensitive) == 0)
        return true;

    if (version.startsWith('v', Qt::CaseInsensitive) &&
        version.mid(1).compare(tag, Qt::CaseInsensitive) == 0)
        return true;

    return false;
}


MainWindow::GitHubReleaseInfo MainWindow::latestReleaseFromJson(
    const QJsonArray &releases,
    bool includePrereleases
    )
{
    GitHubReleaseInfo latest;

    for (const QJsonValue &value : releases) {
        if (!value.isObject())
            continue;

        const QJsonObject release = value.toObject();

        const QString tagName = release.value("tag_name").toString();
        const QString publishedAtString = release.value("published_at").toString();
        const QString htmlUrl = release.value("html_url").toString();
        const bool prerelease = release.value("prerelease").toBool(false);

        if (tagName.isEmpty() || publishedAtString.isEmpty())
            continue;

        // Stable channel: ignore GitHub pre-releases.
        // Unstable channel: allow both full releases and pre-releases.
        if (!includePrereleases && prerelease) {
            qDebug() << "[GitHub] Skipping prerelease because stable channel is selected:"
                     << tagName;
            continue;
        }

        const QDateTime publishedAt =
            QDateTime::fromString(publishedAtString, Qt::ISODate);

        if (!publishedAt.isValid())
            continue;

        qDebug() << "[GitHub] Candidate:"
                 << "tag:" << tagName
                 << "prerelease:" << prerelease
                 << "published_at:" << publishedAt.toString(Qt::ISODate)
                 << "url:" << htmlUrl;

        if (!latest.isValid() || publishedAt > latest.publishedAt) {
            latest.tag = tagName;
            latest.prerelease = prerelease;
            latest.publishedAt = publishedAt;
            latest.htmlUrl = htmlUrl;
        }
    }

    return latest;
};

MainWindow::GitHubReleaseInfo MainWindow::releaseForCurrentVersionFromJson(
    const QJsonArray &releases,
    const QString &currentVersion,
    bool includePrereleases
    )
{
    for (const QJsonValue &value : releases) {
        if (!value.isObject())
            continue;

        const QJsonObject release = value.toObject();

        const QString tagName = release.value("tag_name").toString();

        if (!releaseTagMatchesCurrentVersion(tagName, currentVersion))
            continue;

        const bool prerelease = release.value("prerelease").toBool(false);

        // Stable channel: ignore GitHub pre-releases.
        // Unstable channel: allow both full releases and pre-releases.
        if (!includePrereleases && prerelease) {
            qDebug() << "[GitHub] Skipping current-version prerelease because stable channel is selected:"
                     << tagName;
            continue;
        }

        const QString publishedAtString = release.value("published_at").toString();
        const QString htmlUrl = release.value("html_url").toString();

        const QDateTime publishedAt =
            QDateTime::fromString(publishedAtString, Qt::ISODate);

        if (!publishedAt.isValid())
            continue;

        GitHubReleaseInfo current;
        current.tag = tagName;
        current.prerelease = prerelease;
        current.publishedAt = publishedAt;
        current.htmlUrl = htmlUrl;

        qDebug() << "[GitHub] Current version release found:"
                 << "tag:" << current.tag
                 << "prerelease:" << current.prerelease
                 << "published_at:" << current.publishedAt.toString(Qt::ISODate);

        return current;
    }

    return {};
};
