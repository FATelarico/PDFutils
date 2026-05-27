#ifndef INSERT_SERVICE_H
#define INSERT_SERVICE_H

#include "operation_result.h"

#include <QString>
#include <QStringList>

struct InsertOptions
{
    enum class Mode
    {
        Fixed,
        ManualPages,
        EveryNPages,
        Bookmarks
    };

    QStringList inputPaths;
    QString insertedPdfPath;
    QString outputPath;

    Mode mode = Mode::Fixed;

    // Used when mode == Fixed.
    // 0: insert after every page
    // 1: insert after even pages
    // 2: insert after odd pages
    int fixedModeIndex = 0;

    // Used when mode == ManualPages.
    // Examples: "3,7" or "3,7;2,8" for per-document positions.
    QString manualInsertionPages;

    // Used when mode == EveryNPages.
    int pagesPerInsertion = 1;

    // Used when mode == Bookmarks.
    int bookmarkLevel = 1;
};

OperationResult insertPagesIntoPdfs(
    const InsertOptions& options,
    QStringList* createdFiles = nullptr);

#endif // INSERT_SERVICE_H
