#include <QString>
#include <QStringList>
#include <QVector>
#include <QFileInfo>
#include <QSet>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <exception>
#include <vector>

/*
    Requires:
    - merge_helpers.cpp
    - split_helpers.cpp

    Uses:
    - PageRange
    - DocumentPageSelection
    - toQpdfFileName()
    - makeSplitOutputPath()
    - validateSelectionsAgainstPageCounts()
*/

enum class ExtractOutputMode
{
    OnePdfPerRange,
    OnePdfPerInputFile,
    OnePdfForAllInputFiles
};

static bool resolveExtractRangesFromSelections(
    const QVector<DocumentPageSelection>& selections,
    const QVector<int>& pageCounts,
    QVector<QVector<PageRange>>& extractRangesByDocument,
    QString& error)
{
    extractRangesByDocument.clear();
    error.clear();

    if (selections.size() != pageCounts.size()) {
        error = QString(
                    "Internal error: there are %1 page selection(s), but %2 page count(s)."
                    )
                    .arg(selections.size())
                    .arg(pageCounts.size());
        return false;
    }

    for (int docIndex = 0; docIndex < selections.size(); ++docIndex) {
        const DocumentPageSelection& selection = selections[docIndex];
        const int pageCount = pageCounts[docIndex];

        QVector<PageRange> ranges;

        if (selection.allPages) {
            ranges.append(PageRange{ 1, pageCount });
        }
        else {
            ranges = selection.ranges;
        }

        if (ranges.isEmpty()) {
            error = QString("Document %1 has no extraction ranges.")
            .arg(docIndex + 1);
            extractRangesByDocument.clear();
            return false;
        }

        extractRangesByDocument.append(ranges);
    }

    return true;
}

static bool validateExtractRange(
    const PageRange& range,
    int pageCount,
    int documentNumber,
    int extractNumber,
    QString& error)
{
    error.clear();

    if (pageCount <= 0) {
        error = QString("Document %1 has no readable pages.")
        .arg(documentNumber);
        return false;
    }

    if (range.first < 1 || range.last < 1) {
        error = QString(
                    "Document %1, extract %2 contains invalid page range %3-%4."
                    )
                    .arg(documentNumber)
                    .arg(extractNumber)
                    .arg(range.first)
                    .arg(range.last);
        return false;
    }

    if (range.first > range.last) {
        error = QString(
                    "Document %1, extract %2 contains descending page range %3-%4."
                    )
                    .arg(documentNumber)
                    .arg(extractNumber)
                    .arg(range.first)
                    .arg(range.last);
        return false;
    }

    if (range.last > pageCount) {
        error = QString(
                    "Document %1 has only %2 page(s), but extract %3 requests range %4-%5."
                    )
                    .arg(documentNumber)
                    .arg(pageCount)
                    .arg(extractNumber)
                    .arg(range.first)
                    .arg(range.last);
        return false;
    }

    return true;
}

static bool extractOnePdfWithLibQpdf(
    const QString& inputPdfPath,
    const QVector<PageRange>& ranges,
    const QString& chosenOutputPath,
    int documentNumber,
    QStringList& createdFiles,
    QSet<QString>& plannedOutputPaths,
    QString& error)
{
    error.clear();

    const QString cleanInputPath = inputPdfPath.trimmed();

    if (cleanInputPath.isEmpty()) {
        error = QString("Document %1 path is empty.").arg(documentNumber);
        return false;
    }

    if (ranges.isEmpty()) {
        error = QString("Document %1 has no extraction ranges.")
        .arg(documentNumber);
        return false;
    }

    const QFileInfo inputInfo(cleanInputPath);

    if (!inputInfo.exists()) {
        error = QString("Document %1 does not exist: %2")
        .arg(documentNumber)
            .arg(cleanInputPath);
        return false;
    }

    if (!inputInfo.isFile()) {
        error = QString("Document %1 path is not a file: %2")
        .arg(documentNumber)
            .arg(cleanInputPath);
        return false;
    }

    try {
        QPDF inputPdf;

        const QByteArray inputFileName = toQpdfFileName(cleanInputPath);
        inputPdf.processFile(inputFileName.constData());

        QPDFPageDocumentHelper inputPages(inputPdf);

        const std::vector<QPDFPageObjectHelper> pages =
            inputPages.getAllPages();

        const int pageCount = static_cast<int>(pages.size());

        if (pageCount <= 0) {
            error = QString("Document %1 has no readable pages: %2")
            .arg(documentNumber)
                .arg(cleanInputPath);
            return false;
        }

        for (int rangeIndex = 0; rangeIndex < ranges.size(); ++rangeIndex) {
            const PageRange& range = ranges[rangeIndex];

            QString validationError;

            if (!validateExtractRange(
                    range,
                    pageCount,
                    documentNumber,
                    rangeIndex + 1,
                    validationError)) {
                error = validationError;
                return false;
            }

            QString outputFilePath;
            QString outputPathError;

            /*
                Reuse split-style naming.

                Example:
                input:  report.pdf
                chosen output path: /target/_extract.pdf

                outputs:
                /target/report_extract-1.pdf
                /target/report_extract-2.pdf
                ...
            */
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

            const QString outputKey =
                QFileInfo(outputFilePath).absoluteFilePath();

            if (plannedOutputPaths.contains(outputKey)) {
                error = QString(
                            "Output filename collision: %1. "
                            "This can happen when multiple input PDFs have the same base filename."
                            )
                            .arg(outputFilePath);
                return false;
            }

            plannedOutputPaths.insert(outputKey);

            QPDF outputPdf;
            outputPdf.emptyPDF();

            QPDFPageDocumentHelper outputPages(outputPdf);

            for (int pageNumber = range.first; pageNumber <= range.last; ++pageNumber) {
                const int pageIndex = pageNumber - 1;

                outputPages.addPage(
                    pages.at(static_cast<size_t>(pageIndex)),
                    false
                    );
            }

            const QByteArray outputFileName = toQpdfFileName(outputFilePath);

            QPDFWriter writer(outputPdf, outputFileName.constData());
            writer.write();

            createdFiles.append(outputFilePath);
        }

        return true;
    }
    catch (const std::exception& e) {
        error = QString("qpdf extraction failed for document %1 '%2': %3")
        .arg(documentNumber)
            .arg(cleanInputPath)
            .arg(QString::fromUtf8(e.what()));

        return false;
    }
}

static bool extractPdfsWithLibQpdf(
    const QStringList& pdfPaths,
    const QVector<QVector<PageRange>>& extractRangesByDocument,
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

    if (pdfPaths.size() != extractRangesByDocument.size()) {
        error = QString(
                    "Internal error: there are %1 PDF file(s), but %2 extraction range set(s)."
                    )
                    .arg(pdfPaths.size())
                    .arg(extractRangesByDocument.size());
        return false;
    }

    if (chosenOutputPath.trimmed().isEmpty()) {
        error = "No output path was provided.";
        return false;
    }

    QSet<QString> plannedOutputPaths;

    for (int docIndex = 0; docIndex < pdfPaths.size(); ++docIndex) {
        if (!extractOnePdfWithLibQpdf(
                pdfPaths[docIndex],
                extractRangesByDocument[docIndex],
                chosenOutputPath,
                docIndex + 1,
                createdFiles,
                plannedOutputPaths,
                error)) {
            return false;
        }
    }

    return true;
}

static bool extractPdfsMergedByFileWithLibQpdf(
    const QStringList& pdfPaths,
    const QVector<DocumentPageSelection>& selections,
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

    if (pdfPaths.size() != selections.size()) {
        error = QString(
                    "Internal error: there are %1 PDF file(s), but %2 page selection(s)."
                    )
                    .arg(pdfPaths.size())
                    .arg(selections.size());
        return false;
    }

    if (chosenOutputPath.trimmed().isEmpty()) {
        error = "No output path was provided.";
        return false;
    }

    QSet<QString> plannedOutputPaths;

    for (int docIndex = 0; docIndex < pdfPaths.size(); ++docIndex) {
        QString outputFilePath;
        QString outputPathError;

        /*
            One output per input PDF.

            Example with chosen path /target/_extract.pdf:

            doc 1: inputA_extract-1.pdf
            doc 2: inputB_extract-2.pdf
            doc 3: inputC_extract-3.pdf

            If there are 10+ files, numbering is padded.
        */
        if (!makeSplitOutputPath(
                pdfPaths[docIndex],
                chosenOutputPath,
                docIndex + 1,
                pdfPaths.size(),
                outputFilePath,
                outputPathError)) {
            error = outputPathError;
            createdFiles.clear();
            return false;
        }

        const QString outputKey =
            QFileInfo(outputFilePath).absoluteFilePath();

        if (plannedOutputPaths.contains(outputKey)) {
            error = QString("Output filename collision: %1")
            .arg(outputFilePath);
            createdFiles.clear();
            return false;
        }

        plannedOutputPaths.insert(outputKey);

        QStringList onePdfPath;
        onePdfPath.append(pdfPaths[docIndex]);

        QVector<DocumentPageSelection> oneSelection;
        oneSelection.append(selections[docIndex]);

        QString mergeError;

        if (!mergeSelectedPagesWithLibQpdf(
                onePdfPath,
                oneSelection,
                outputFilePath,
                mergeError)) {
            error = QString("Document %1: %2")
            .arg(docIndex + 1)
                .arg(mergeError);
            createdFiles.clear();
            return false;
        }

        createdFiles.append(outputFilePath);
    }

    return true;
}

static bool extractPdfsMergedGloballyWithLibQpdf(
    const QStringList& pdfPaths,
    const QVector<DocumentPageSelection>& selections,
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

    if (pdfPaths.size() != selections.size()) {
        error = QString(
                    "Internal error: there are %1 PDF file(s), but %2 page selection(s)."
                    )
                    .arg(pdfPaths.size())
                    .arg(selections.size());
        return false;
    }

    if (chosenOutputPath.trimmed().isEmpty()) {
        error = "No output path was provided.";
        return false;
    }

    const QString finalOutputPath =
        normalisedPdfPath(chosenOutputPath);

    if (!mergeSelectedPagesWithLibQpdf(
            pdfPaths,
            selections,
            finalOutputPath,
            error)) {
        createdFiles.clear();
        return false;
    }

    createdFiles.append(finalOutputPath);
    return true;
}