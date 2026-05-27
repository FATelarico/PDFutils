#pragma once

#include <QDateTime>
#include <QString>

struct GitHubReleaseInfo {
    QString tag;
    bool prerelease = false;
    QDateTime publishedAt;
    QString htmlUrl;

    bool isValid() const
    {
        return !tag.isEmpty() && publishedAt.isValid();
    }
};
