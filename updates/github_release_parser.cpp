#include "github_release_parser.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1Char>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

namespace {

    struct PreReleaseIdentifier
    {
        bool isNumeric = false;
        int numericValue = 0;
        QString textValue;
    };

    struct ParsedReleaseVersion
    {
        bool isValid = false;
        QVector<int> coreNumbers;
        QVector<PreReleaseIdentifier> prereleaseIdentifiers;
    };

    QString normalisedVersionTag(QString value)
    {
        value = value.trimmed();

        if (value.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
            value.remove(0, 1);

        value.replace(QLatin1Char('~'), QLatin1Char('-'));

        return value;
    }

    bool isDigitsOnly(const QString& value)
    {
        if (value.isEmpty())
            return false;

        for (const QChar character : value) {
            if (!character.isDigit())
                return false;
        }

        return true;
    }

    ParsedReleaseVersion parseReleaseVersion(const QString& rawTag)
    {
        ParsedReleaseVersion parsed;

        const QString tag = normalisedVersionTag(rawTag);

        if (tag.isEmpty())
            return parsed;

        const int prereleaseSeparatorIndex = tag.indexOf(QLatin1Char('-'));
        const QString coreText =
            prereleaseSeparatorIndex >= 0 ? tag.left(prereleaseSeparatorIndex) : tag;
        const QString prereleaseText =
            prereleaseSeparatorIndex >= 0 ? tag.mid(prereleaseSeparatorIndex + 1) : QString();

        const QStringList coreParts =
            coreText.split(QLatin1Char('.'), Qt::SkipEmptyParts);

        if (coreParts.isEmpty())
            return parsed;

        for (const QString& corePart : coreParts) {
            if (!isDigitsOnly(corePart))
                return parsed;

            parsed.coreNumbers.append(corePart.toInt());
        }

        if (!prereleaseText.isEmpty()) {
            const QStringList prereleaseParts = prereleaseText.split(
                QRegularExpression(QStringLiteral("[._-]+")),
                Qt::SkipEmptyParts
            );

            for (const QString& prereleasePart : prereleaseParts) {
                PreReleaseIdentifier identifier;

                if (isDigitsOnly(prereleasePart)) {
                    identifier.isNumeric = true;
                    identifier.numericValue = prereleasePart.toInt();
                } else {
                    identifier.textValue = prereleasePart.toLower();
                }

                parsed.prereleaseIdentifiers.append(identifier);
            }
        }

        parsed.isValid = true;
        return parsed;
    }

    int comparePreReleaseIdentifiers(
        const QVector<PreReleaseIdentifier>& left,
        const QVector<PreReleaseIdentifier>& right)
    {
        const int sharedSize = qMin(left.size(), right.size());

        for (int i = 0; i < sharedSize; ++i) {
            const PreReleaseIdentifier& leftIdentifier = left.at(i);
            const PreReleaseIdentifier& rightIdentifier = right.at(i);

            if (leftIdentifier.isNumeric && rightIdentifier.isNumeric) {
                if (leftIdentifier.numericValue < rightIdentifier.numericValue)
                    return -1;

                if (leftIdentifier.numericValue > rightIdentifier.numericValue)
                    return 1;

                continue;
            }

            if (leftIdentifier.isNumeric != rightIdentifier.isNumeric)
                return leftIdentifier.isNumeric ? -1 : 1;

            const int textComparison = QString::compare(
                leftIdentifier.textValue,
                rightIdentifier.textValue,
                Qt::CaseInsensitive
            );

            if (textComparison < 0)
                return -1;

            if (textComparison > 0)
                return 1;
        }

        if (left.size() < right.size())
            return -1;

        if (left.size() > right.size())
            return 1;

        return 0;
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

int compareReleaseTags(
    const QString& leftTag,
    const QString& rightTag,
    bool* ok)
{
    const ParsedReleaseVersion left = parseReleaseVersion(leftTag);
    const ParsedReleaseVersion right = parseReleaseVersion(rightTag);

    const bool comparisonOk = left.isValid && right.isValid;

    if (ok)
        *ok = comparisonOk;

    if (!comparisonOk)
        return 0;

    const int sharedCoreSize = qMax(left.coreNumbers.size(), right.coreNumbers.size());

    for (int i = 0; i < sharedCoreSize; ++i) {
        const int leftNumber = i < left.coreNumbers.size() ? left.coreNumbers.at(i) : 0;
        const int rightNumber = i < right.coreNumbers.size() ? right.coreNumbers.at(i) : 0;

        if (leftNumber < rightNumber)
            return -1;

        if (leftNumber > rightNumber)
            return 1;
    }

    const bool leftHasPrerelease = !left.prereleaseIdentifiers.isEmpty();
    const bool rightHasPrerelease = !right.prereleaseIdentifiers.isEmpty();

    if (leftHasPrerelease != rightHasPrerelease)
        return leftHasPrerelease ? -1 : 1;

    return comparePreReleaseIdentifiers(
        left.prereleaseIdentifiers,
        right.prereleaseIdentifiers
    );
}

bool releaseTagMatchesCurrentVersion(
    const QString &releaseTag,
    const QString &currentVersion
)
{
    const QString tag = normalisedVersionTag(releaseTag);
    const QString version = normalisedVersionTag(currentVersion);

    if (tag.isEmpty() || version.isEmpty())
        return false;

    bool comparisonOk = false;
    const int comparison = compareReleaseTags(tag, version, &comparisonOk);

    if (comparisonOk)
        return comparison == 0;

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

        if (!latest.isValid()) {
            latest = candidate;
            continue;
        }

        bool comparisonOk = false;
        const int comparison = compareReleaseTags(candidate.tag, latest.tag, &comparisonOk);

        if (comparisonOk) {
            if (comparison > 0 ||
                (comparison == 0 && candidate.publishedAt > latest.publishedAt)) {
                latest = candidate;
            }
            continue;
        }

        if (candidate.publishedAt > latest.publishedAt)
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
