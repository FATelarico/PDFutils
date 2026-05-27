#pragma once

#include "github_release_info.h"

#include <QDateTime>
#include <QString>
#include <QUrl>

struct GitHubUpdateCheckOptions {
    QString owner = QStringLiteral("fatelarico");
    QString repo = QStringLiteral("PDFutils");

    QString currentVersion;
    QDateTime fallbackCurrentReleaseDate;

    bool includePrereleases = false;
    int timeoutMilliseconds = 15000;

    QString userAgent = QStringLiteral("PDFutils");
};

struct GitHubUpdateCheckResult {
    bool ok = false;
    QString errorMessage;

    int httpStatus = 0;

    QString owner;
    QString repo;
    QString currentVersion;

    QUrl apiUrl;
    QUrl latestReleaseUrl;

    GitHubReleaseInfo latestRelease;
    GitHubReleaseInfo currentRelease;

    QDateTime currentReleaseDate;
    bool currentReleaseDateFromApi = false;

    bool updateAvailable = false;
};

GitHubUpdateCheckResult checkGitHubForUpdates(
    const GitHubUpdateCheckOptions& options
);
