#ifndef SPLIT_SERVICE_H
#define SPLIT_SERVICE_H

#include "operation_result.h"

#include <QString>
#include <QStringList>

struct SplitOptions
{
    enum class Mode
    {
        Fixed,
        ManualSplitPoints,
        EveryNPages,
        Bookmarks
    };

    QStringList inputPaths;
    QString outputPath;

    Mode mode = Mode::Fixed;

    // Used when mode == Fixed.
    // 0: one output per page
    // 1: two-page chunks starting at pages 1, 3, 5...
    // 2: split after pages 1, 3, 5...
    int fixedModeIndex = 0;

    // Used when mode == ManualSplitPoints.
    // Examples: "3,7,12" or "3,7;2,4" for per-document points.
    QString manualSplitPoints;

    // Used when mode == EveryNPages.
    int pagesPerPart = 1;

    // Used when mode == Bookmarks.
    int bookmarkLevel = 1;
};

OperationResult splitPdfs(
    const SplitOptions& options,
    QStringList* createdFiles = nullptr);

#endif // SPLIT_SERVICE_H
