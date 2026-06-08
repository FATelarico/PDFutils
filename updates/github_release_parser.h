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

int compareReleaseTags(
    const QString& leftTag,
    const QString& rightTag,
    bool* ok = nullptr
);

bool releaseTagMatchesCurrentVersion(
    const QString &releaseTag,
    const QString &currentVersion
);
