#pragma once

#include "github_release_info.h"

#include <QJsonArray>
#include <QString>

GitHubReleaseInfo latestReleaseFromJson(
    const QJsonArray &releases,
    bool includePrereleases
);

GitHubReleaseInfo releaseForCurrentVersionFromJson(
    const QJsonArray &releases,
    const QString &currentVersion,
    bool includePrereleases
);

bool releaseTagMatchesCurrentVersion(
    const QString &releaseTag,
    const QString &currentVersion
);
