#include "split_service.h"

#include "page_range.h"
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
QVector<PageRange> makeOnePageSplitRanges(
    int pageCount,
    QString& error)
{
    QVector<PageRange> ranges;
    error.clear();

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return ranges;
    }

    for (int page = 1; page <= pageCount; ++page) {
        ranges.append(PageRange{ page, page });
    }

    return ranges;
}

QVector<PageRange> makeTwoPageSplitRangesAfterEvenPages(
    int pageCount,
    QString& error)
{
    QVector<PageRange> ranges;
    error.clear();

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return ranges;
    }

    for (int first = 1; first <= pageCount; first += 2) {
        const int last = std::min(first + 1, pageCount);
        ranges.append(PageRange{ first, last });
    }

    return ranges;
}

QVector<PageRange> makeTwoPageSplitRangesAfterOddPages(
    int pageCount,
    QString& error)
{
    QVector<PageRange> ranges;
    error.clear();

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return ranges;
    }

    ranges.append(PageRange{ 1, 1 });

    for (int first = 2; first <= pageCount; first += 2) {
        const int last = std::min(first + 1, pageCount);
        ranges.append(PageRange{ first, last });
    }

    return ranges;
}

bool parsePositiveIntegerToken(
    const QString& token,
    int& value,
    QString& error)
{
    error.clear();
    value = 0;

    const QString cleanToken = token.trimmed();

    if (cleanToken.isEmpty()) {
        error = "Empty page number in split-point list.";
        return false;
    }

    bool ok = false;
    const int parsedValue = cleanToken.toInt(&ok);

    if (!ok || parsedValue <= 0) {
        error = QString("Invalid split page number: '%1'.").arg(cleanToken);
        return false;
    }

    value = parsedValue;
    return true;
}

bool parseCommaSeparatedSplitPoints(
    const QString& input,
    QVector<int>& splitPoints,
    QString& error)
{
    splitPoints.clear();
    error.clear();

    const QString cleanInput = input.trimmed();

    if (cleanInput.isEmpty()) {
        error = "Split points cannot be empty.";
        return false;
    }

    const QStringList tokens = cleanInput.split(',');

    QSet<int> seen;

    for (const QString& token : tokens) {
        int page = 0;
        QString tokenError;

        if (!parsePositiveIntegerToken(token, page, tokenError)) {
            error = tokenError;
            return false;
        }

        if (seen.contains(page)) {
            error = QString("Duplicate split page number: %1.").arg(page);
            return false;
        }

        seen.insert(page);
        splitPoints.append(page);
    }

    std::sort(splitPoints.begin(), splitPoints.end());
    return true;
}

bool splitPointsToRanges(
    const QVector<int>& splitPoints,
    int pageCount,
    QVector<PageRange>& ranges,
    QString& error)
{
    ranges.clear();
    error.clear();

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return false;
    }

    if (splitPoints.isEmpty()) {
        error = "Split points cannot be empty.";
        return false;
    }

    int firstPage = 1;

    for (int splitPoint : splitPoints) {
        if (splitPoint < 1) {
            error = QString("Invalid split page number: %1.").arg(splitPoint);
            return false;
        }

        if (splitPoint >= pageCount) {
            error = QString(
                "Split page %1 is invalid because the document has only %2 page(s). "
                "A split point must be between page 1 and page %3."
            )
            .arg(splitPoint)
            .arg(pageCount)
            .arg(pageCount - 1);
            return false;
        }

        if (splitPoint < firstPage) {
            error = QString("Invalid repeated or descending split page: %1.").arg(splitPoint);
            return false;
        }

        ranges.append(PageRange{ firstPage, splitPoint });
        firstPage = splitPoint + 1;
    }

    ranges.append(PageRange{ firstPage, pageCount });
    return true;
}

bool determineSplitRangesFromManualSplitPoints(
    const QString& input,
    const QVector<int>& pageCounts,
    QVector<QVector<PageRange>>& splitRangesByDocument,
    QString& error)
{
    splitRangesByDocument.clear();
    error.clear();

    const QString cleanInput = input.trimmed();

    if (cleanInput.isEmpty()) {
        error = "Split points cannot be empty.";
        return false;
    }

    if (pageCounts.isEmpty()) {
        error = "No PDF page counts were provided.";
        return false;
    }

    const QStringList documentSpecs = cleanInput.split(';');

    if (documentSpecs.size() != 1 && documentSpecs.size() != pageCounts.size()) {
        error = QString(
            "Invalid number of split-point groups. For %1 document(s), provide either "
            "one comma-separated list used for all documents, or exactly %1 semicolon-separated "
            "list(s), meaning %2 semicolon(s)."
        )
        .arg(pageCounts.size())
        .arg(pageCounts.size() - 1);
        return false;
    }

    QVector<QVector<int>> splitPointsByDocument;

    if (documentSpecs.size() == 1) {
        QVector<int> commonSplitPoints;
        QString parseError;

        if (!parseCommaSeparatedSplitPoints(documentSpecs[0], commonSplitPoints, parseError)) {
            error = parseError;
            return false;
        }

        for (int i = 0; i < pageCounts.size(); ++i) {
            splitPointsByDocument.append(commonSplitPoints);
        }
    }
    else {
        for (int docIndex = 0; docIndex < documentSpecs.size(); ++docIndex) {
            QVector<int> splitPoints;
            QString parseError;

            if (!parseCommaSeparatedSplitPoints(documentSpecs[docIndex], splitPoints, parseError)) {
                error = QString("Document %1: %2").arg(docIndex + 1).arg(parseError);
                return false;
            }

            splitPointsByDocument.append(splitPoints);
        }
    }

    for (int docIndex = 0; docIndex < pageCounts.size(); ++docIndex) {
        QVector<PageRange> ranges;
        QString rangeError;

        if (!splitPointsToRanges(splitPointsByDocument[docIndex], pageCounts[docIndex], ranges, rangeError)) {
            error = QString("Document %1: %2").arg(docIndex + 1).arg(rangeError);
            splitRangesByDocument.clear();
            return false;
        }

        splitRangesByDocument.append(ranges);
    }

    return true;
}

bool makeEveryNPagesSplitRanges(
    int pageCount,
    int pagesPerPart,
    QVector<PageRange>& ranges,
    QString& error)
{
    ranges.clear();
    error.clear();

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return false;
    }

    if (pagesPerPart <= 0) {
        error = "Number of pages per split part must be greater than zero.";
        return false;
    }

    for (int first = 1; first <= pageCount; first += pagesPerPart) {
        const int last = std::min(first + pagesPerPart - 1, pageCount);
        ranges.append(PageRange{ first, last });
    }

    return true;
}

bool determineSplitRangesEveryNPages(
    int pagesPerPart,
    const QVector<int>& pageCounts,
    QVector<QVector<PageRange>>& splitRangesByDocument,
    QString& error)
{
    splitRangesByDocument.clear();
    error.clear();

    if (pageCounts.isEmpty()) {
        error = "No PDF page counts were provided.";
        return false;
    }

    if (pagesPerPart <= 0) {
        error = "Number of pages per split part must be greater than zero.";
        return false;
    }

    for (int docIndex = 0; docIndex < pageCounts.size(); ++docIndex) {
        QVector<PageRange> ranges;
        QString localError;

        if (!makeEveryNPagesSplitRanges(pageCounts[docIndex], pagesPerPart, ranges, localError)) {
            error = QString("Document %1: %2").arg(docIndex + 1).arg(localError);
            splitRangesByDocument.clear();
            return false;
        }

        splitRangesByDocument.append(ranges);
    }

    return true;
}

std::pair<int, int> qpdfObjectKey(const QPDFObjectHandle& object)
{
    const QPDFObjGen objGen = object.getObjGen();

    return std::make_pair(objGen.getObj(), objGen.getGen());
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
    const std::vector<QPDFPageObjectHelper> pages = pageHelper.getAllPages();

    pageCount = static_cast<int>(pages.size());

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return false;
    }

    for (int i = 0; i < pageCount; ++i) {
        const QPDFObjectHandle pageObject = pages.at(static_cast<size_t>(i)).getObjectHandle();
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
        // const QPDFObjectHandle destPage = outline.getDestPage();
        QPDFObjectHandle destPage = outline.getDestPage();

        if (!destPage.isNull()) {
            const auto pageIt = pageNumberByObject.find(qpdfObjectKey(destPage));

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
        const int distance = level > requestedLevel ? level - requestedLevel : requestedLevel - level;

        if (selectedLevel == 0 ||
            distance < bestDistance ||
            (distance == bestDistance && level < selectedLevel)) {
            selectedLevel = level;
            bestDistance = distance;
        }
    }

    return true;
}

bool bookmarkTargetPagesToSplitRanges(
    const QVector<int>& bookmarkTargetPages,
    int pageCount,
    QVector<PageRange>& ranges,
    QString& error)
{
    ranges.clear();
    error.clear();

    if (pageCount <= 0) {
        error = "PDF has no readable pages.";
        return false;
    }

    if (bookmarkTargetPages.isEmpty()) {
        error = "No bookmark target pages were found.";
        return false;
    }

    QVector<int> splitPoints;
    QSet<int> seenSplitPoints;

    for (int targetPage : bookmarkTargetPages) {
        if (targetPage <= 1) {
            continue;
        }

        if (targetPage > pageCount) {
            error = QString(
                "Bookmark target page %1 is outside the document, which has only %2 page(s)."
            )
            .arg(targetPage)
            .arg(pageCount);
            return false;
        }

        const int splitPoint = targetPage - 1;

        if (!seenSplitPoints.contains(splitPoint)) {
            seenSplitPoints.insert(splitPoint);
            splitPoints.append(splitPoint);
        }
    }

    if (splitPoints.isEmpty()) {
        error =
            "The selected bookmark level does not create any usable split points. "
            "This usually means all matching bookmarks point to page 1.";
        return false;
    }

    std::sort(splitPoints.begin(), splitPoints.end());

    return splitPointsToRanges(splitPoints, pageCount, ranges, error);
}

bool determineSplitRangesFromBookmarksForOnePdf(
    const QString& pdfPath,
    int requestedLevel,
    QVector<PageRange>& ranges,
    QString& error)
{
    ranges.clear();
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

        // QPDFOutlineDocumentHelper& outlineHelper = QPDFOutlineDocumentHelper::get(pdf);
        QPDFOutlineDocumentHelper outlineHelper(pdf);

        if (!outlineHelper.hasOutlines()) {
            error = "The PDF has no bookmarks.";
            return false;
        }

        std::map<std::pair<int, int>, int> pageNumberByObject;
        int pageCount = 0;
        QString pageMapError;

        if (!buildQpdfPageNumberMap(pdf, pageNumberByObject, pageCount, pageMapError)) {
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

        if (!chooseClosestBookmarkLevel(targetPagesByLevel, requestedLevel, selectedLevel, levelError)) {
            error = levelError;
            return false;
        }

        const auto levelIt = targetPagesByLevel.find(selectedLevel);

        if (levelIt == targetPagesByLevel.end()) {
            error = QString("Could not select a usable bookmark level for requested level %1.").arg(requestedLevel);
            return false;
        }

        QString rangeError;

        if (!bookmarkTargetPagesToSplitRanges(levelIt->second, pageCount, ranges, rangeError)) {
            if (selectedLevel == requestedLevel) {
                error = rangeError;
            }
            else {
                error = QString(
                    "Requested bookmark level %1 was not found; closest usable level was %2. %3"
                )
                .arg(requestedLevel)
                .arg(selectedLevel)
                .arg(rangeError);
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

bool determineSplitRangesFromBookmarks(
    const QStringList& pdfPaths,
    int requestedLevel,
    const QVector<int>& pageCounts,
    QVector<QVector<PageRange>>& splitRangesByDocument,
    QString& error)
{
    splitRangesByDocument.clear();
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
        error = QString(
            "Internal error: there are %1 PDF file(s), but %2 page count(s)."
        )
        .arg(pdfPaths.size())
        .arg(pageCounts.size());
        return false;
    }

    for (int docIndex = 0; docIndex < pdfPaths.size(); ++docIndex) {
        QVector<PageRange> ranges;
        QString localError;

        if (!determineSplitRangesFromBookmarksForOnePdf(pdfPaths[docIndex], requestedLevel, ranges, localError)) {
            error = QString("Document %1: %2").arg(docIndex + 1).arg(localError);
            splitRangesByDocument.clear();
            return false;
        }

        splitRangesByDocument.append(ranges);
    }

    return true;
}

bool determineSplitRanges(
    const SplitOptions& options,
    const QVector<int>& pageCounts,
    QVector<QVector<PageRange>>& splitRangesByDocument,
    QString& error)
{
    splitRangesByDocument.clear();
    error.clear();

    if (pageCounts.isEmpty()) {
        error = "No PDF page counts were provided.";
        return false;
    }

    switch (options.mode) {
    case SplitOptions::Mode::Bookmarks:
        return determineSplitRangesFromBookmarks(
            options.inputPaths,
            options.bookmarkLevel,
            pageCounts,
            splitRangesByDocument,
            error
        );

    case SplitOptions::Mode::EveryNPages:
        return determineSplitRangesEveryNPages(
            options.pagesPerPart,
            pageCounts,
            splitRangesByDocument,
            error
        );

    case SplitOptions::Mode::ManualSplitPoints:
        return determineSplitRangesFromManualSplitPoints(
            options.manualSplitPoints,
            pageCounts,
            splitRangesByDocument,
            error
        );

    case SplitOptions::Mode::Fixed:
        break;
    }

    for (int docIndex = 0; docIndex < pageCounts.size(); ++docIndex) {
        const int pageCount = pageCounts[docIndex];

        QString localError;
        QVector<PageRange> ranges;

        switch (options.fixedModeIndex) {
        case 0:
            ranges = makeOnePageSplitRanges(pageCount, localError);
            break;

        case 1:
            ranges = makeTwoPageSplitRangesAfterEvenPages(pageCount, localError);
            break;

        case 2:
            ranges = makeTwoPageSplitRangesAfterOddPages(pageCount, localError);
            break;

        default:
            error = QString("Unknown split mode: %1.").arg(options.fixedModeIndex);
            return false;
        }

        if (!localError.isEmpty()) {
            error = QString("Document %1: %2").arg(docIndex + 1).arg(localError);
            splitRangesByDocument.clear();
            return false;
        }

        if (ranges.isEmpty()) {
            error = QString("Document %1 produced no split ranges.").arg(docIndex + 1);
            splitRangesByDocument.clear();
            return false;
        }

        splitRangesByDocument.append(ranges);
    }

    return true;
}

QString normalisedPdfPath(const QString& path)
{
    QString result = path.trimmed();

    if (!result.endsWith(".pdf", Qt::CaseInsensitive)) {
        result += ".pdf";
    }

    return result;
}

bool makeSplitOutputPath(
    const QString& inputPdfPath,
    const QString& chosenOutputPath,
    int pieceNumber,
    int pieceCount,
    QString& splitOutputPath,
    QString& error)
{
    splitOutputPath.clear();
    error.clear();

    if (inputPdfPath.trimmed().isEmpty()) {
        error = "Input PDF path is empty.";
        return false;
    }

    if (chosenOutputPath.trimmed().isEmpty()) {
        error = "Output path is empty.";
        return false;
    }

    if (pieceNumber <= 0) {
        error = QString("Invalid split piece number: %1.").arg(pieceNumber);
        return false;
    }

    if (pieceCount <= 0) {
        error = QString("Invalid split piece count: %1.").arg(pieceCount);
        return false;
    }

    if (pieceNumber > pieceCount) {
        error = QString("Split piece number %1 exceeds total piece count %2.")
            .arg(pieceNumber)
            .arg(pieceCount);
        return false;
    }

    const QFileInfo inputInfo(inputPdfPath.trimmed());
    const QFileInfo chosenOutputInfo(normalisedPdfPath(chosenOutputPath));

    const QString inputBaseName = inputInfo.completeBaseName();
    const QString outputSuffix = chosenOutputInfo.completeBaseName();

    if (inputBaseName.trimmed().isEmpty()) {
        error = QString("Could not determine input file base name from: %1").arg(inputPdfPath);
        return false;
    }

    if (outputSuffix.trimmed().isEmpty()) {
        error = QString("Could not determine output file suffix from: %1").arg(chosenOutputPath);
        return false;
    }

    QDir outputDir = chosenOutputInfo.absoluteDir();

    if (!outputDir.exists()) {
        if (!QDir().mkpath(outputDir.absolutePath())) {
            error = QString("Could not create output directory: %1").arg(outputDir.absolutePath());
            return false;
        }
    }

    const int digitWidth = QString::number(pieceCount).length();
    const QString paddedPieceNumber = QString("%1").arg(pieceNumber, digitWidth, 10, QLatin1Char('0'));

    const QString outputFileName = QString("%1%2-%3.pdf")
        .arg(inputBaseName)
        .arg(outputSuffix)
        .arg(paddedPieceNumber);

    splitOutputPath = outputDir.filePath(outputFileName);
    return true;
}

bool validateSplitRange(
    const PageRange& range,
    int pageCount,
    int documentNumber,
    QString& error)
{
    error.clear();

    if (pageCount <= 0) {
        error = QString("Document %1 has no readable pages.").arg(documentNumber);
        return false;
    }

    if (range.first < 1 || range.last < 1) {
        error = QString("Document %1 contains invalid page range %2-%3.")
            .arg(documentNumber)
            .arg(range.first)
            .arg(range.last);
        return false;
    }

    if (range.first > range.last) {
        error = QString("Document %1 contains descending page range %2-%3.")
            .arg(documentNumber)
            .arg(range.first)
            .arg(range.last);
        return false;
    }

    if (range.last > pageCount) {
        error = QString(
            "Document %1 has only %2 page(s), but range %3-%4 was requested."
        )
        .arg(documentNumber)
        .arg(pageCount)
        .arg(range.first)
        .arg(range.last);
        return false;
    }

    return true;
}

bool splitOnePdfWithLibQpdf(
    const QString& inputPdfPath,
    const QVector<PageRange>& ranges,
    const QString& chosenOutputPath,
    int documentNumber,
    QStringList& createdFiles,
    QString& error)
{
    error.clear();

    const QString cleanInputPath = inputPdfPath.trimmed();

    if (cleanInputPath.isEmpty()) {
        error = QString("Document %1 path is empty.").arg(documentNumber);
        return false;
    }

    if (ranges.isEmpty()) {
        error = QString("Document %1 has no split ranges.").arg(documentNumber);
        return false;
    }

    const QFileInfo inputInfo(cleanInputPath);

    if (!inputInfo.exists()) {
        error = QString("Document %1 does not exist: %2").arg(documentNumber).arg(cleanInputPath);
        return false;
    }

    if (!inputInfo.isFile()) {
        error = QString("Document %1 path is not a file: %2").arg(documentNumber).arg(cleanInputPath);
        return false;
    }

    try {
        QPDF inputPdf;

        const QByteArray inputFileName = toQpdfFileName(cleanInputPath);
        inputPdf.processFile(inputFileName.constData());

        QPDFPageDocumentHelper inputPages(inputPdf);
        const std::vector<QPDFPageObjectHelper> pages = inputPages.getAllPages();

        const int pageCount = static_cast<int>(pages.size());

        if (pageCount <= 0) {
            error = QString("Document %1 has no readable pages: %2").arg(documentNumber).arg(cleanInputPath);
            return false;
        }

        for (int rangeIndex = 0; rangeIndex < ranges.size(); ++rangeIndex) {
            const PageRange& range = ranges[rangeIndex];

            QString validationError;

            if (!validateSplitRange(range, pageCount, documentNumber, validationError)) {
                error = validationError;
                return false;
            }

            QString outputFilePath;
            QString outputPathError;

            if (!makeSplitOutputPath(
                    cleanInputPath,
                    chosenOutputPath,
                    rangeIndex + 1,
                    ranges.size(),
                    outputFilePath,
                    outputPathError)) {
                error = outputPathError;
                return false;
            }

            QPDF outputPdf;
            outputPdf.emptyPDF();

            QPDFPageDocumentHelper outputPages(outputPdf);

            for (int pageNumber = range.first; pageNumber <= range.last; ++pageNumber) {
                const int pageIndex = pageNumber - 1;
                outputPages.addPage(pages.at(static_cast<size_t>(pageIndex)), false);
            }

            const QByteArray outputFileName = toQpdfFileName(outputFilePath);
            QPDFWriter writer(outputPdf, outputFileName.constData());
            writer.write();

            createdFiles.append(outputFilePath);
        }

        return true;
    }
    catch (const std::exception& e) {
        error = QString("qpdf split failed for document %1 '%2': %3")
            .arg(documentNumber)
            .arg(cleanInputPath)
            .arg(QString::fromUtf8(e.what()));
        return false;
    }
}

bool splitPdfsWithLibQpdf(
    const QStringList& pdfPaths,
    const QVector<QVector<PageRange>>& splitRangesByDocument,
    const QString& chosenOutputPath,
    QStringList& createdFiles,
    QString& error)
{
    createdFiles.clear();
    error.clear();

    if (pdfPaths.isEmpty()) {
        error = "No PDF files were provided.";
        return false;
    }

    if (pdfPaths.size() != splitRangesByDocument.size()) {
        error = QString(
            "Internal error: there are %1 PDF file(s), but %2 split range set(s)."
        )
        .arg(pdfPaths.size())
        .arg(splitRangesByDocument.size());
        return false;
    }

    if (chosenOutputPath.trimmed().isEmpty()) {
        error = "No output path was provided.";
        return false;
    }

    for (int docIndex = 0; docIndex < pdfPaths.size(); ++docIndex) {
        if (!splitOnePdfWithLibQpdf(
                pdfPaths[docIndex],
                splitRangesByDocument[docIndex],
                chosenOutputPath,
                docIndex + 1,
                createdFiles,
                error)) {
            createdFiles.clear();
            return false;
        }
    }

    return true;
}

bool validateSplitRangesAgainstPageCounts(
    const QVector<QVector<PageRange>>& splitRangesByDocument,
    const QVector<int>& pageCounts,
    QString& error)
{
    error.clear();

    if (splitRangesByDocument.size() != pageCounts.size()) {
        error = QString(
            "Internal error: there are %1 split range set(s), but %2 page count(s)."
        )
        .arg(splitRangesByDocument.size())
        .arg(pageCounts.size());
        return false;
    }

    for (int docIndex = 0; docIndex < splitRangesByDocument.size(); ++docIndex) {
        const int pageCount = pageCounts[docIndex];

        if (pageCount <= 0) {
            error = QString("Document %1 has no readable pages.").arg(docIndex + 1);
            return false;
        }

        const QVector<PageRange>& ranges = splitRangesByDocument[docIndex];

        if (ranges.isEmpty()) {
            error = QString("Document %1 has no split ranges.").arg(docIndex + 1);
            return false;
        }

        int expectedFirstPage = 1;

        for (int rangeIndex = 0; rangeIndex < ranges.size(); ++rangeIndex) {
            const PageRange& range = ranges[rangeIndex];

            if (range.first < 1 || range.last < 1) {
                error = QString(
                    "Document %1, output part %2 has invalid range %3-%4."
                )
                .arg(docIndex + 1)
                .arg(rangeIndex + 1)
                .arg(range.first)
                .arg(range.last);
                return false;
            }

            if (range.first > range.last) {
                error = QString(
                    "Document %1, output part %2 has descending range %3-%4."
                )
                .arg(docIndex + 1)
                .arg(rangeIndex + 1)
                .arg(range.first)
                .arg(range.last);
                return false;
            }

            if (range.last > pageCount) {
                error = QString(
                    "Document %1 has only %2 page(s), but output part %3 requests range %4-%5."
                )
                .arg(docIndex + 1)
                .arg(pageCount)
                .arg(rangeIndex + 1)
                .arg(range.first)
                .arg(range.last);
                return false;
            }

            if (range.first != expectedFirstPage) {
                error = QString(
                    "Document %1 has non-contiguous split ranges. "
                    "Output part %2 starts at page %3, but page %4 was expected."
                )
                .arg(docIndex + 1)
                .arg(rangeIndex + 1)
                .arg(range.first)
                .arg(expectedFirstPage);
                return false;
            }

            expectedFirstPage = range.last + 1;
        }

        if (expectedFirstPage != pageCount + 1) {
            error = QString(
                "Document %1 split ranges do not cover the whole document. "
                "Expected coverage through page %2, but coverage stopped at page %3."
            )
            .arg(docIndex + 1)
            .arg(pageCount)
            .arg(expectedFirstPage - 1);
            return false;
        }
    }

    return true;
}
} // namespace

OperationResult splitPdfs(
    const SplitOptions& options,
    QStringList* createdFilesOut)
{
    if (createdFilesOut) {
        createdFilesOut->clear();
    }

    if (options.inputPaths.isEmpty()) {
        return OperationResult::failure("No PDF files were provided.");
    }

    if (options.outputPath.trimmed().isEmpty()) {
        return OperationResult::failure("No output path was provided.");
    }

    QVector<int> pageCounts;
    OperationResult pageCountsResult = getPdfPageCounts(options.inputPaths, pageCounts);

    if (!pageCountsResult.ok) {
        return pageCountsResult;
    }

    QVector<QVector<PageRange>> splitRangesByDocument;
    QString rangeError;

    if (!determineSplitRanges(options, pageCounts, splitRangesByDocument, rangeError)) {
        return OperationResult::failure(rangeError);
    }

    QString validationError;

    if (!validateSplitRangesAgainstPageCounts(splitRangesByDocument, pageCounts, validationError)) {
        return OperationResult::failure(validationError);
    }

    QStringList createdFiles;
    QString splitError;

    if (!splitPdfsWithLibQpdf(
            options.inputPaths,
            splitRangesByDocument,
            options.outputPath,
            createdFiles,
            splitError)) {
        return OperationResult::failure(splitError);
    }

    if (createdFilesOut) {
        *createdFilesOut = createdFiles;
    }

    return OperationResult::success(
        QString("Created %1 PDF file(s).").arg(createdFiles.size())
    );
}
