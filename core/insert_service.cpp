#include "insert_service.h"

#include "operation_result.h"
#include "qpdf_utils.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFOutlineObjectHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <algorithm>
#include <exception>
#include <map>
#include <utility>
#include <vector>

namespace
{

bool parsePositivePageNumberToken(
    const QString& token,
    int& value,
    QString& error)
{
    error.clear();
    value = 0;

    const QString cleanToken = token.trimmed();

    if (cleanToken.isEmpty()) {
        error = "Empty page number in page-number list.";
        return false;
    }

    bool ok = false;
    const int parsedValue = cleanToken.toInt(&ok);

    if (!ok || parsedValue <= 0) {
        error = QString("Invalid page number: '%1'.").arg(cleanToken);
        return false;
    }

    value = parsedValue;
    return true;
}

bool parseCommaSeparatedPageNumbers(
    const QString& input,
    QVector<int>& pageNumbers,
    QString& error)
{
    pageNumbers.clear();
    error.clear();

    const QString cleanInput = input.trimmed();

    if (cleanInput.isEmpty()) {
        error = "Page-number list cannot be empty.";
        return false;
    }

    const QStringList tokens = cleanInput.split(',', Qt::KeepEmptyParts);
    QSet<int> seen;

    for (const QString& token : tokens) {
        int page = 0;
        QString tokenError;

        if (!parsePositivePageNumberToken(token, page, tokenError)) {
            error = tokenError;
            return false;
        }

        if (seen.contains(page)) {
            error = QString("Duplicate page number: %1.").arg(page);
            return false;
        }

        seen.insert(page);
        pageNumbers.append(page);
    }

    std::sort(pageNumbers.begin(), pageNumbers.end());
    return true;
}

bool validateInsertionPositions(
    const QVector<int>& insertionPositions,
    int pageCount,
    int documentNumber,
    QString& error)
{
    error.clear();

    if (pageCount <= 0) {
        error = QString("Document %1 has no readable pages.")
            .arg(documentNumber);
        return false;
    }

    if (insertionPositions.isEmpty()) {
        error = QString("Document %1 has no insertion positions.")
            .arg(documentNumber);
        return false;
    }

    int previousPosition = -1;

    for (int i = 0; i < insertionPositions.size(); ++i) {
        const int position = insertionPositions[i];

        if (position < 0) {
            error = QString("Document %1 contains invalid insertion position %2.")
                .arg(documentNumber)
                .arg(position);
            return false;
        }

        if (position > pageCount) {
            error = QString("Document %1 has only %2 page(s), but insertion after page %3 was requested.")
                .arg(documentNumber)
                .arg(pageCount)
                .arg(position);
            return false;
        }

        if (position == previousPosition) {
            error = QString("Document %1 contains duplicate insertion position %2.")
                .arg(documentNumber)
                .arg(position);
            return false;
        }

        if (position < previousPosition) {
            error = QString("Document %1 insertion positions must be sorted in ascending order.")
                .arg(documentNumber);
            return false;
        }

        previousPosition = position;
    }

    return true;
}

bool validateInsertionPositionsAgainstPageCounts(
    const QVector<QVector<int>>& insertionPositionsByDocument,
    const QVector<int>& pageCounts,
    QString& error)
{
    error.clear();

    if (insertionPositionsByDocument.size() != pageCounts.size()) {
        error = QString("Internal error: there are %1 insertion-position set(s), but %2 page count(s).")
            .arg(insertionPositionsByDocument.size())
            .arg(pageCounts.size());
        return false;
    }

    for (int docIndex = 0; docIndex < insertionPositionsByDocument.size(); ++docIndex) {
        QString localError;

        if (!validateInsertionPositions(
                insertionPositionsByDocument[docIndex],
                pageCounts[docIndex],
                docIndex + 1,
                localError)) {
            error = localError;
            return false;
        }
    }

    return true;
}

QVector<int> makeAfterEveryPageInsertionPositions(
    int pageCount,
    QString& error)
{
    QVector<int> positions;
    error.clear();

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return positions;
    }

    for (int page = 1; page <= pageCount; ++page) {
        positions.append(page);
    }

    return positions;
}

QVector<int> makeAfterEvenPagesInsertionPositions(
    int pageCount,
    QString& error)
{
    QVector<int> positions;
    error.clear();

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return positions;
    }

    for (int page = 2; page <= pageCount; page += 2) {
        positions.append(page);
    }

    if (positions.isEmpty()) {
        error = "The document has no even page after which pages can be inserted.";
    }

    return positions;
}

QVector<int> makeAfterOddPagesInsertionPositions(
    int pageCount,
    QString& error)
{
    QVector<int> positions;
    error.clear();

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return positions;
    }

    for (int page = 1; page <= pageCount; page += 2) {
        positions.append(page);
    }

    return positions;
}

bool makeEveryNPagesInsertionPositions(
    int pageCount,
    int pagesPerInsertion,
    QVector<int>& positions,
    QString& error)
{
    positions.clear();
    error.clear();

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return false;
    }

    if (pagesPerInsertion <= 0) {
        error = "Number of pages between insertions must be greater than zero.";
        return false;
    }

    for (int page = pagesPerInsertion; page <= pageCount; page += pagesPerInsertion) {
        positions.append(page);
    }

    if (positions.isEmpty()) {
        error = QString(
            "No insertion positions were produced because the document has only %1 page(s), "
            "but insertion every %2 page(s) was requested.")
            .arg(pageCount)
            .arg(pagesPerInsertion);
        return false;
    }

    return true;
}

bool determineInsertionPositionsEveryNPages(
    int pagesPerInsertion,
    const QVector<int>& pageCounts,
    QVector<QVector<int>>& insertionPositionsByDocument,
    QString& error)
{
    insertionPositionsByDocument.clear();
    error.clear();

    if (pageCounts.isEmpty()) {
        error = "No PDF page counts were provided.";
        return false;
    }

    for (int docIndex = 0; docIndex < pageCounts.size(); ++docIndex) {
        QVector<int> positions;
        QString localError;

        if (!makeEveryNPagesInsertionPositions(
                pageCounts[docIndex],
                pagesPerInsertion,
                positions,
                localError)) {
            error = QString("Document %1: %2")
                .arg(docIndex + 1)
                .arg(localError);
            insertionPositionsByDocument.clear();
            return false;
        }

        insertionPositionsByDocument.append(positions);
    }

    return true;
}

bool determineInsertionPositionsFromManualPages(
    const QString& input,
    const QVector<int>& pageCounts,
    QVector<QVector<int>>& insertionPositionsByDocument,
    QString& error)
{
    insertionPositionsByDocument.clear();
    error.clear();

    const QString cleanInput = input.trimmed();

    if (cleanInput.isEmpty()) {
        error = "Insertion page list cannot be empty.";
        return false;
    }

    if (pageCounts.isEmpty()) {
        error = "No PDF page counts were provided.";
        return false;
    }

    const QStringList documentSpecs = cleanInput.split(';', Qt::KeepEmptyParts);

    if (documentSpecs.size() != 1 && documentSpecs.size() != pageCounts.size()) {
        error = QString(
            "Invalid number of insertion-page groups. For %1 document(s), provide either "
            "one comma-separated list used for all documents, or exactly %1 semicolon-separated "
            "list(s), meaning %2 semicolon(s).")
            .arg(pageCounts.size())
            .arg(pageCounts.size() - 1);
        return false;
    }

    if (documentSpecs.size() == 1) {
        QVector<int> sharedPositions;
        QString parseError;

        if (!parseCommaSeparatedPageNumbers(
                documentSpecs[0],
                sharedPositions,
                parseError)) {
            error = parseError;
            return false;
        }

        for (int i = 0; i < pageCounts.size(); ++i) {
            insertionPositionsByDocument.append(sharedPositions);
        }
    }
    else {
        for (int docIndex = 0; docIndex < documentSpecs.size(); ++docIndex) {
            QVector<int> positions;
            QString parseError;

            if (!parseCommaSeparatedPageNumbers(
                    documentSpecs[docIndex],
                    positions,
                    parseError)) {
                error = QString("Document %1: %2")
                    .arg(docIndex + 1)
                    .arg(parseError);
                insertionPositionsByDocument.clear();
                return false;
            }

            insertionPositionsByDocument.append(positions);
        }
    }

    QString validationError;

    if (!validateInsertionPositionsAgainstPageCounts(
            insertionPositionsByDocument,
            pageCounts,
            validationError)) {
        error = validationError;
        insertionPositionsByDocument.clear();
        return false;
    }

    return true;
}

std::pair<int, int> qpdfObjectKey(const QPDFObjectHandle& object)
{
    const QPDFObjGen objGen = object.getObjGen();

    return std::make_pair(
        objGen.getObj(),
        objGen.getGen()
    );
}

bool buildQpdfPageNumberMap(
    QPDF& pdf,
    std::map<std::pair<int, int>, int>& pageNumberByObject,
    int& pageCount,
    QString& error)
{
    pageNumberByObject.clear();
    pageCount = 0;
    error.clear();

    QPDFPageDocumentHelper pageHelper(pdf);

    const std::vector<QPDFPageObjectHelper> pages =
        pageHelper.getAllPages();

    pageCount = static_cast<int>(pages.size());

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return false;
    }

    for (int i = 0; i < pageCount; ++i) {
        const QPDFObjectHandle pageObject =
            pages.at(static_cast<size_t>(i)).getObjectHandle();

        pageNumberByObject[qpdfObjectKey(pageObject)] = i + 1;
    }

    return true;
}

void collectBookmarkTargetPagesByLevel(
    std::vector<QPDFOutlineObjectHelper> outlines,
    int level,
    const std::map<std::pair<int, int>, int>& pageNumberByObject,
    std::map<int, QVector<int>>& targetPagesByLevel)
{
    for (QPDFOutlineObjectHelper outline : outlines) {
        const QPDFObjectHandle destPage = outline.getDestPage();

        if (!destPage.isNull()) {
            const auto pageIt =
                pageNumberByObject.find(qpdfObjectKey(destPage));

            if (pageIt != pageNumberByObject.end()) {
                targetPagesByLevel[level].append(pageIt->second);
            }
        }

        collectBookmarkTargetPagesByLevel(
            outline.getKids(),
            level + 1,
            pageNumberByObject,
            targetPagesByLevel
        );
    }
}

bool chooseClosestBookmarkLevel(
    const std::map<int, QVector<int>>& targetPagesByLevel,
    int requestedLevel,
    int& selectedLevel,
    QString& error)
{
    selectedLevel = 0;
    error.clear();

    if (requestedLevel <= 0) {
        error = "Bookmark level must be greater than zero.";
        return false;
    }

    if (targetPagesByLevel.empty()) {
        error = "The PDF has bookmarks, but none of them points to a page that qpdf can resolve.";
        return false;
    }

    const auto exactIt = targetPagesByLevel.find(requestedLevel);

    if (exactIt != targetPagesByLevel.end()) {
        selectedLevel = requestedLevel;
        return true;
    }

    int bestDistance = -1;

    for (const auto& entry : targetPagesByLevel) {
        const int level = entry.first;

        const int distance =
            level > requestedLevel
                ? level - requestedLevel
                : requestedLevel - level;

        if (selectedLevel == 0 ||
            distance < bestDistance ||
            (distance == bestDistance && level < selectedLevel)) {
            selectedLevel = level;
            bestDistance = distance;
        }
    }

    return true;
}

bool bookmarkTargetPagesToInsertionPositions(
    const QVector<int>& bookmarkTargetPages,
    int pageCount,
    QVector<int>& insertionPositions,
    QString& error)
{
    insertionPositions.clear();
    error.clear();

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return false;
    }

    if (bookmarkTargetPages.isEmpty()) {
        error = "No bookmark target pages were found.";
        return false;
    }

    QSet<int> seenPositions;

    for (int targetPage : bookmarkTargetPages) {
        if (targetPage < 1) {
            error = QString("Invalid bookmark target page: %1.").arg(targetPage);
            return false;
        }

        if (targetPage > pageCount) {
            error = QString("Bookmark target page %1 is outside the document, which has only %2 page(s).")
                .arg(targetPage)
                .arg(pageCount);
            return false;
        }

        const int insertionPosition = targetPage - 1;

        if (!seenPositions.contains(insertionPosition)) {
            seenPositions.insert(insertionPosition);
            insertionPositions.append(insertionPosition);
        }
    }

    if (insertionPositions.isEmpty()) {
        error = "The selected bookmark level produced no usable insertion positions.";
        return false;
    }

    std::sort(insertionPositions.begin(), insertionPositions.end());
    return true;
}

bool determineInsertionPositionsFromBookmarksForOnePdf(
    const QString& pdfPath,
    int requestedLevel,
    QVector<int>& insertionPositions,
    QString& error)
{
    insertionPositions.clear();
    error.clear();

    if (requestedLevel <= 0) {
        error = "Bookmark level must be greater than zero.";
        return false;
    }

    const QString cleanPath = pdfPath.trimmed();

    if (cleanPath.isEmpty()) {
        error = "PDF path is empty.";
        return false;
    }

    try {
        QPDF pdf;

        const QByteArray inputFileName = toQpdfFileName(cleanPath);
        pdf.processFile(inputFileName.constData());

        QPDFOutlineDocumentHelper& outlineHelper =
            QPDFOutlineDocumentHelper::get(pdf);

        if (!outlineHelper.hasOutlines()) {
            error = "The PDF has no bookmarks.";
            return false;
        }

        std::map<std::pair<int, int>, int> pageNumberByObject;
        int pageCount = 0;
        QString pageMapError;

        if (!buildQpdfPageNumberMap(
                pdf,
                pageNumberByObject,
                pageCount,
                pageMapError)) {
            error = pageMapError;
            return false;
        }

        std::map<int, QVector<int>> targetPagesByLevel;

        collectBookmarkTargetPagesByLevel(
            outlineHelper.getTopLevelOutlines(),
            1,
            pageNumberByObject,
            targetPagesByLevel
        );

        int selectedLevel = 0;
        QString levelError;

        if (!chooseClosestBookmarkLevel(
                targetPagesByLevel,
                requestedLevel,
                selectedLevel,
                levelError)) {
            error = levelError;
            return false;
        }

        const auto levelIt = targetPagesByLevel.find(selectedLevel);

        if (levelIt == targetPagesByLevel.end()) {
            error = QString("Could not select a usable bookmark level for requested level %1.")
                .arg(requestedLevel);
            return false;
        }

        QString positionsError;

        if (!bookmarkTargetPagesToInsertionPositions(
                levelIt->second,
                pageCount,
                insertionPositions,
                positionsError)) {
            if (selectedLevel == requestedLevel) {
                error = positionsError;
            }
            else {
                error = QString("Requested bookmark level %1 was not found; closest usable level was %2. %3")
                    .arg(requestedLevel)
                    .arg(selectedLevel)
                    .arg(positionsError);
            }

            return false;
        }

        return true;
    }
    catch (const std::exception& e) {
        error = QString("qpdf bookmark inspection failed for '%1': %2")
            .arg(cleanPath)
            .arg(QString::fromUtf8(e.what()));

        return false;
    }
}

bool determineInsertionPositionsFromBookmarks(
    const QStringList& pdfPaths,
    int requestedLevel,
    const QVector<int>& pageCounts,
    QVector<QVector<int>>& insertionPositionsByDocument,
    QString& error)
{
    insertionPositionsByDocument.clear();
    error.clear();

    if (requestedLevel <= 0) {
        error = "Bookmark level must be greater than zero.";
        return false;
    }

    if (pdfPaths.isEmpty()) {
        error = "No PDF files were provided.";
        return false;
    }

    if (pdfPaths.size() != pageCounts.size()) {
        error = QString("Internal error: there are %1 PDF file(s), but %2 page count(s).")
            .arg(pdfPaths.size())
            .arg(pageCounts.size());
        return false;
    }

    for (int docIndex = 0; docIndex < pdfPaths.size(); ++docIndex) {
        QVector<int> positions;
        QString localError;

        if (!determineInsertionPositionsFromBookmarksForOnePdf(
                pdfPaths[docIndex],
                requestedLevel,
                positions,
                localError)) {
            error = QString("Document %1: %2")
                .arg(docIndex + 1)
                .arg(localError);
            insertionPositionsByDocument.clear();
            return false;
        }

        insertionPositionsByDocument.append(positions);
    }

    return true;
}

bool determineInsertionPositionsFromOptions(
    const InsertOptions& options,
    const QVector<int>& pageCounts,
    QVector<QVector<int>>& insertionPositionsByDocument,
    QString& error)
{
    insertionPositionsByDocument.clear();
    error.clear();

    if (pageCounts.isEmpty()) {
        error = "No PDF page counts were provided.";
        return false;
    }

    if (options.mode == InsertOptions::Mode::Bookmarks) {
        return determineInsertionPositionsFromBookmarks(
            options.inputPaths,
            options.bookmarkLevel,
            pageCounts,
            insertionPositionsByDocument,
            error
        );
    }

    if (options.mode == InsertOptions::Mode::EveryNPages) {
        return determineInsertionPositionsEveryNPages(
            options.pagesPerInsertion,
            pageCounts,
            insertionPositionsByDocument,
            error
        );
    }

    if (options.mode == InsertOptions::Mode::ManualPages) {
        return determineInsertionPositionsFromManualPages(
            options.manualInsertionPages,
            pageCounts,
            insertionPositionsByDocument,
            error
        );
    }

    for (int docIndex = 0; docIndex < pageCounts.size(); ++docIndex) {
        const int pageCount = pageCounts[docIndex];

        QString localError;
        QVector<int> positions;

        switch (options.fixedModeIndex) {
        case 0:
            positions = makeAfterEveryPageInsertionPositions(pageCount, localError);
            break;

        case 1:
            positions = makeAfterEvenPagesInsertionPositions(pageCount, localError);
            break;

        case 2:
            positions = makeAfterOddPagesInsertionPositions(pageCount, localError);
            break;

        default:
            error = QString("Unknown insertion mode: %1.").arg(options.fixedModeIndex);
            return false;
        }

        if (!localError.isEmpty()) {
            error = QString("Document %1: %2")
                .arg(docIndex + 1)
                .arg(localError);
            insertionPositionsByDocument.clear();
            return false;
        }

        if (positions.isEmpty()) {
            error = QString("Document %1 produced no insertion positions.")
                .arg(docIndex + 1);
            insertionPositionsByDocument.clear();
            return false;
        }

        insertionPositionsByDocument.append(positions);
    }

    QString validationError;

    if (!validateInsertionPositionsAgainstPageCounts(
            insertionPositionsByDocument,
            pageCounts,
            validationError)) {
        error = validationError;
        insertionPositionsByDocument.clear();
        return false;
    }

    return true;
}

QString cleanAbsolutePathForComparison(const QString& path)
{
    const QFileInfo fileInfo(path.trimmed());
    return QDir::cleanPath(fileInfo.absoluteFilePath());
}

bool makeSplitStyleOutputPath(
    const QString& inputPdfPath,
    const QString& chosenOutputPath,
    int pieceNumber,
    int pieceCount,
    QString& outputPath,
    QString& error)
{
    outputPath.clear();
    error.clear();

    if (inputPdfPath.trimmed().isEmpty()) {
        error = "Input PDF path is empty.";
        return false;
    }

    if (chosenOutputPath.trimmed().isEmpty()) {
        error = "Output path is empty.";
        return false;
    }

    if (pieceNumber <= 0 || pieceCount <= 0 || pieceNumber > pieceCount) {
        error = QString("Invalid output numbering: item %1 of %2.")
            .arg(pieceNumber)
            .arg(pieceCount);
        return false;
    }

    const QFileInfo inputInfo(inputPdfPath.trimmed());
    const QFileInfo chosenOutputInfo(ensurePdfExtension(chosenOutputPath));

    const QString inputBaseName = inputInfo.completeBaseName();
    const QString outputSuffix = chosenOutputInfo.completeBaseName();

    if (inputBaseName.trimmed().isEmpty()) {
        error = QString("Could not determine input file base name from: %1")
            .arg(inputPdfPath);
        return false;
    }

    if (outputSuffix.trimmed().isEmpty()) {
        error = QString("Could not determine output file suffix from: %1")
            .arg(chosenOutputPath);
        return false;
    }

    QDir outputDir = chosenOutputInfo.absoluteDir();

    if (!outputDir.exists()) {
        if (!QDir().mkpath(outputDir.absolutePath())) {
            error = QString("Could not create output directory: %1")
                .arg(outputDir.absolutePath());
            return false;
        }
    }

    const int digitWidth = QString::number(pieceCount).length();

    const QString paddedPieceNumber =
        QString("%1").arg(pieceNumber, digitWidth, 10, QLatin1Char('0'));

    const QString outputFileName =
        QString("%1%2-%3.pdf")
            .arg(inputBaseName)
            .arg(outputSuffix)
            .arg(paddedPieceNumber);

    outputPath = outputDir.filePath(outputFileName);
    return true;
}

bool makeInsertionOutputPath(
    const QString& inputPdfPath,
    const QString& chosenOutputPath,
    int documentNumber,
    int documentCount,
    QString& insertionOutputPath,
    QString& error)
{
    insertionOutputPath.clear();
    error.clear();

    if (inputPdfPath.trimmed().isEmpty()) {
        error = "Input PDF path is empty.";
        return false;
    }

    if (chosenOutputPath.trimmed().isEmpty()) {
        error = "Output path is empty.";
        return false;
    }

    if (documentNumber <= 0 || documentCount <= 0 || documentNumber > documentCount) {
        error = QString("Invalid document numbering: document %1 of %2.")
            .arg(documentNumber)
            .arg(documentCount);
        return false;
    }

    if (documentCount == 1) {
        insertionOutputPath = ensurePdfExtension(chosenOutputPath);

        QDir outputDir = QFileInfo(insertionOutputPath).absoluteDir();

        if (!outputDir.exists()) {
            if (!QDir().mkpath(outputDir.absolutePath())) {
                error = QString("Could not create output directory: %1")
                    .arg(outputDir.absolutePath());
                insertionOutputPath.clear();
                return false;
            }
        }

        return true;
    }

    return makeSplitStyleOutputPath(
        inputPdfPath,
        chosenOutputPath,
        documentNumber,
        documentCount,
        insertionOutputPath,
        error
    );
}

void appendPagesToOutput(
    QPDFPageDocumentHelper& outputPages,
    const std::vector<QPDFPageObjectHelper>& pages)
{
    for (const QPDFPageObjectHelper& page : pages) {
        outputPages.addPage(page, false);
    }
}

bool insertPagesIntoOnePdfWithLibQpdf(
    const QString& inputPdfPath,
    const QString& insertedPdfPath,
    const QVector<int>& insertionPositions,
    const QString& chosenOutputPath,
    int documentNumber,
    int documentCount,
    QString& createdFile,
    QString& error)
{
    createdFile.clear();
    error.clear();

    const QString cleanInputPath = inputPdfPath.trimmed();
    const QString cleanInsertedPath = insertedPdfPath.trimmed();

    if (cleanInputPath.isEmpty()) {
        error = QString("Document %1 path is empty.").arg(documentNumber);
        return false;
    }

    if (cleanInsertedPath.isEmpty()) {
        error = "Inserted PDF path is empty.";
        return false;
    }

    if (insertionPositions.isEmpty()) {
        error = QString("Document %1 has no insertion positions.")
            .arg(documentNumber);
        return false;
    }

    const QFileInfo inputInfo(cleanInputPath);
    const QFileInfo insertedInfo(cleanInsertedPath);

    if (!inputInfo.exists() || !inputInfo.isFile()) {
        error = QString("Document %1 is not a readable file: %2")
            .arg(documentNumber)
            .arg(cleanInputPath);
        return false;
    }

    if (!insertedInfo.exists() || !insertedInfo.isFile()) {
        error = QString("Inserted PDF is not a readable file: %1")
            .arg(cleanInsertedPath);
        return false;
    }

    QString outputFilePath;
    QString outputPathError;

    if (!makeInsertionOutputPath(
            cleanInputPath,
            chosenOutputPath,
            documentNumber,
            documentCount,
            outputFilePath,
            outputPathError)) {
        error = outputPathError;
        return false;
    }

    const QString outputKey = cleanAbsolutePathForComparison(outputFilePath);
    const QString inputKey = cleanAbsolutePathForComparison(cleanInputPath);
    const QString insertedKey = cleanAbsolutePathForComparison(cleanInsertedPath);

    if (outputKey == inputKey) {
        error = QString("Refusing to overwrite input document %1. Choose a different output path.")
            .arg(cleanInputPath);
        return false;
    }

    if (outputKey == insertedKey) {
        error = QString("Refusing to overwrite the inserted PDF %1. Choose a different output path.")
            .arg(cleanInsertedPath);
        return false;
    }

    try {
        QPDF inputPdf;
        const QByteArray inputFileName = toQpdfFileName(cleanInputPath);
        inputPdf.processFile(inputFileName.constData());

        QPDF insertedPdf;
        const QByteArray insertedFileName = toQpdfFileName(cleanInsertedPath);
        insertedPdf.processFile(insertedFileName.constData());

        QPDFPageDocumentHelper inputPageHelper(inputPdf);
        QPDFPageDocumentHelper insertedPageHelper(insertedPdf);

        const std::vector<QPDFPageObjectHelper> inputPages =
            inputPageHelper.getAllPages();
        const std::vector<QPDFPageObjectHelper> insertedPages =
            insertedPageHelper.getAllPages();

        const int inputPageCount = static_cast<int>(inputPages.size());
        const int insertedPageCount = static_cast<int>(insertedPages.size());

        if (inputPageCount <= 0) {
            error = QString("Document %1 has no readable pages: %2")
                .arg(documentNumber)
                .arg(cleanInputPath);
            return false;
        }

        if (insertedPageCount <= 0) {
            error = QString("Inserted PDF has no readable pages: %1")
                .arg(cleanInsertedPath);
            return false;
        }

        QString validationError;

        if (!validateInsertionPositions(
                insertionPositions,
                inputPageCount,
                documentNumber,
                validationError)) {
            error = validationError;
            return false;
        }

        QPDF outputPdf;
        outputPdf.emptyPDF();

        QPDFPageDocumentHelper outputPages(outputPdf);

        int positionIndex = 0;

        while (positionIndex < insertionPositions.size() &&
               insertionPositions[positionIndex] == 0) {
            appendPagesToOutput(outputPages, insertedPages);
            ++positionIndex;
        }

        for (int pageNumber = 1; pageNumber <= inputPageCount; ++pageNumber) {
            outputPages.addPage(
                inputPages.at(static_cast<size_t>(pageNumber - 1)),
                false
            );

            while (positionIndex < insertionPositions.size() &&
                   insertionPositions[positionIndex] == pageNumber) {
                appendPagesToOutput(outputPages, insertedPages);
                ++positionIndex;
            }
        }

        if (positionIndex != insertionPositions.size()) {
            error = QString("Internal error: document %1 has unapplied insertion positions.")
                .arg(documentNumber);
            return false;
        }

        const QByteArray outputFileName = toQpdfFileName(outputFilePath);

        QPDFWriter writer(outputPdf, outputFileName.constData());
        writer.write();

        createdFile = outputFilePath;
        return true;
    }
    catch (const std::exception& e) {
        error = QString("qpdf insertion failed for document %1 '%2': %3")
            .arg(documentNumber)
            .arg(cleanInputPath)
            .arg(QString::fromUtf8(e.what()));
        return false;
    }
}

bool insertPagesIntoPdfsWithLibQpdf(
    const QStringList& inputPdfPaths,
    const QString& insertedPdfPath,
    const QVector<QVector<int>>& insertionPositionsByDocument,
    const QString& chosenOutputPath,
    QStringList& createdFiles,
    QString& error)
{
    createdFiles.clear();
    error.clear();

    if (inputPdfPaths.isEmpty()) {
        error = "No PDF files were provided.";
        return false;
    }

    if (insertedPdfPath.trimmed().isEmpty()) {
        error = "No inserted PDF was provided.";
        return false;
    }

    if (inputPdfPaths.size() != insertionPositionsByDocument.size()) {
        error = QString("Internal error: there are %1 PDF file(s), but %2 insertion-position set(s).")
            .arg(inputPdfPaths.size())
            .arg(insertionPositionsByDocument.size());
        return false;
    }

    if (chosenOutputPath.trimmed().isEmpty()) {
        error = "No output path was provided.";
        return false;
    }

    QSet<QString> plannedOutputPaths;

    for (int docIndex = 0; docIndex < inputPdfPaths.size(); ++docIndex) {
        QString plannedOutputPath;
        QString outputPathError;

        if (!makeInsertionOutputPath(
                inputPdfPaths[docIndex],
                chosenOutputPath,
                docIndex + 1,
                inputPdfPaths.size(),
                plannedOutputPath,
                outputPathError)) {
            error = outputPathError;
            return false;
        }

        const QString outputKey = cleanAbsolutePathForComparison(plannedOutputPath);

        if (plannedOutputPaths.contains(outputKey)) {
            error = QString("Output filename collision: %1")
                .arg(plannedOutputPath);
            return false;
        }

        plannedOutputPaths.insert(outputKey);
    }

    for (int docIndex = 0; docIndex < inputPdfPaths.size(); ++docIndex) {
        QString createdFile;
        QString localError;

        if (!insertPagesIntoOnePdfWithLibQpdf(
                inputPdfPaths[docIndex],
                insertedPdfPath,
                insertionPositionsByDocument[docIndex],
                chosenOutputPath,
                docIndex + 1,
                inputPdfPaths.size(),
                createdFile,
                localError)) {
            error = localError;
            createdFiles.clear();
            return false;
        }

        createdFiles.append(createdFile);
    }

    return true;
}

} // namespace

OperationResult insertPagesIntoPdfs(
    const InsertOptions& options,
    QStringList* createdFiles)
{
    if (createdFiles) {
        createdFiles->clear();
    }

    if (options.inputPaths.isEmpty()) {
        return OperationResult::failure("No PDF files were provided.");
    }

    if (options.insertedPdfPath.trimmed().isEmpty()) {
        return OperationResult::failure("No inserted PDF was provided.");
    }

    if (options.outputPath.trimmed().isEmpty()) {
        return OperationResult::failure("No output path was provided.");
    }

    QVector<int> pageCounts;
    const OperationResult pageCountResult = getPdfPageCounts(options.inputPaths, pageCounts);

    if (!pageCountResult.ok) {
        return pageCountResult;
    }

    int insertedPageCount = 0;
    const OperationResult insertedPageCountResult = getPdfPageCount(
        options.insertedPdfPath,
        insertedPageCount
    );

    if (!insertedPageCountResult.ok) {
        return OperationResult::failure(
            QString("Could not read inserted PDF: %1")
                .arg(insertedPageCountResult.message)
        );
    }

    QVector<QVector<int>> insertionPositionsByDocument;
    QString positionError;

    if (!determineInsertionPositionsFromOptions(
            options,
            pageCounts,
            insertionPositionsByDocument,
            positionError)) {
        return OperationResult::failure(positionError);
    }

    QString validationError;

    if (!validateInsertionPositionsAgainstPageCounts(
            insertionPositionsByDocument,
            pageCounts,
            validationError)) {
        return OperationResult::failure(validationError);
    }

    QStringList localCreatedFiles;
    QString insertError;

    if (!insertPagesIntoPdfsWithLibQpdf(
            options.inputPaths,
            options.insertedPdfPath,
            insertionPositionsByDocument,
            options.outputPath,
            localCreatedFiles,
            insertError)) {
        return OperationResult::failure(insertError);
    }

    if (createdFiles) {
        *createdFiles = localCreatedFiles;
    }

    return OperationResult::success(
        QString("Created %1 PDF file(s).").arg(localCreatedFiles.size())
    );
}
