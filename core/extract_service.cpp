#include "extract_service.h"

#include "page_range.h"
#include "qpdf_utils.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QVector>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <exception>
#include <vector>

namespace
{
QVector<PageRange> rangesFromSelection(
    const DocumentPageSelection& selection,
    int pageCount)
{
    QVector<PageRange> ranges;

    if (selection.allPages) {
        ranges.append(PageRange{1, pageCount});
        return ranges;
    }

    return selection.ranges;
}

OperationResult resolveExtractRangesFromSelections(
    const QVector<DocumentPageSelection>& selections,
    const QVector<int>& pageCounts,
    QVector<QVector<PageRange>>& extractRangesByDocument)
{
    extractRangesByDocument.clear();

    if (selections.size() != pageCounts.size()) {
        return OperationResult::failure(
            QString("Internal error: there are %1 page selection(s), but %2 page count(s).")
                .arg(selections.size())
                .arg(pageCounts.size())
        );
    }

    for (int docIndex = 0; docIndex < selections.size(); ++docIndex) {
        const QVector<PageRange> ranges = rangesFromSelection(
            selections.at(docIndex),
            pageCounts.at(docIndex)
        );

        if (ranges.isEmpty()) {
            return OperationResult::failure(
                QString("Document %1 has no extraction ranges.").arg(docIndex + 1)
            );
        }

        extractRangesByDocument.append(ranges);
    }

    return OperationResult::success();
}

QString normalisedPdfPath(const QString& path)
{
    return ensurePdfExtension(path.trimmed());
}

OperationResult makeExtractOutputPath(
    const QString& inputPdfPath,
    const QString& chosenOutputPath,
    int pieceNumber,
    int pieceCount,
    QString& extractOutputPath)
{
    extractOutputPath.clear();

    if (inputPdfPath.trimmed().isEmpty()) {
        return OperationResult::failure("Input PDF path is empty.");
    }

    if (chosenOutputPath.trimmed().isEmpty()) {
        return OperationResult::failure("Output path is empty.");
    }

    if (pieceNumber <= 0) {
        return OperationResult::failure(
            QString("Invalid extract piece number: %1.").arg(pieceNumber)
        );
    }

    if (pieceCount <= 0) {
        return OperationResult::failure(
            QString("Invalid extract piece count: %1.").arg(pieceCount)
        );
    }

    if (pieceNumber > pieceCount) {
        return OperationResult::failure(
            QString("Extract piece number %1 exceeds total piece count %2.")
                .arg(pieceNumber)
                .arg(pieceCount)
        );
    }

    const QFileInfo inputInfo(inputPdfPath.trimmed());
    const QFileInfo chosenOutputInfo(normalisedPdfPath(chosenOutputPath));

    const QString inputBaseName = inputInfo.completeBaseName();
    const QString outputSuffix = chosenOutputInfo.completeBaseName();

    if (inputBaseName.trimmed().isEmpty()) {
        return OperationResult::failure(
            QString("Could not determine input file base name from: %1").arg(inputPdfPath)
        );
    }

    if (outputSuffix.trimmed().isEmpty()) {
        return OperationResult::failure(
            QString("Could not determine output file suffix from: %1").arg(chosenOutputPath)
        );
    }

    QDir outputDir = chosenOutputInfo.absoluteDir();

    if (!outputDir.exists()) {
        if (!QDir().mkpath(outputDir.absolutePath())) {
            return OperationResult::failure(
                QString("Could not create output directory: %1").arg(outputDir.absolutePath())
            );
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

    extractOutputPath = outputDir.filePath(outputFileName);
    return OperationResult::success();
}

OperationResult validateExtractRange(
    const PageRange& range,
    int pageCount,
    int documentNumber,
    int extractNumber)
{
    if (pageCount <= 0) {
        return OperationResult::failure(
            QString("Document %1 has no readable pages.").arg(documentNumber)
        );
    }

    if (range.first < 1 || range.last < 1) {
        return OperationResult::failure(
            QString("Document %1, extract %2 contains invalid page range %3-%4.")
                .arg(documentNumber)
                .arg(extractNumber)
                .arg(range.first)
                .arg(range.last)
        );
    }

    if (range.first > range.last) {
        return OperationResult::failure(
            QString("Document %1, extract %2 contains descending page range %3-%4.")
                .arg(documentNumber)
                .arg(extractNumber)
                .arg(range.first)
                .arg(range.last)
        );
    }

    if (range.last > pageCount) {
        return OperationResult::failure(
            QString("Document %1 has only %2 page(s), but extract %3 requests range %4-%5.")
                .arg(documentNumber)
                .arg(pageCount)
                .arg(extractNumber)
                .arg(range.first)
                .arg(range.last)
        );
    }

    return OperationResult::success();
}

OperationResult appendRangeToOutputPdf(
    const std::vector<QPDFPageObjectHelper>& pages,
    const PageRange& range,
    QPDFPageDocumentHelper& outputPages)
{
    for (int pageNumber = range.first; pageNumber <= range.last; ++pageNumber) {
        const int pageIndex = pageNumber - 1;

        if (pageIndex < 0 || pageIndex >= static_cast<int>(pages.size())) {
            return OperationResult::failure(
                QString("Requested page %1 is outside the readable page range.").arg(pageNumber)
            );
        }

        outputPages.addPage(
            pages.at(static_cast<size_t>(pageIndex)),
            false
        );
    }

    return OperationResult::success();
}

OperationResult extractOnePdfPerRange(
    const QString& inputPdfPath,
    const QVector<PageRange>& ranges,
    const QString& chosenOutputPath,
    int documentNumber,
    QStringList& createdFiles,
    QSet<QString>& plannedOutputPaths)
{
    const QString cleanInputPath = inputPdfPath.trimmed();

    if (cleanInputPath.isEmpty()) {
        return OperationResult::failure(
            QString("Document %1 path is empty.").arg(documentNumber)
        );
    }

    if (ranges.isEmpty()) {
        return OperationResult::failure(
            QString("Document %1 has no extraction ranges.").arg(documentNumber)
        );
    }

    const QFileInfo inputInfo(cleanInputPath);

    if (!inputInfo.exists()) {
        return OperationResult::failure(
            QString("Document %1 does not exist: %2")
                .arg(documentNumber)
                .arg(cleanInputPath)
        );
    }

    if (!inputInfo.isFile()) {
        return OperationResult::failure(
            QString("Document %1 path is not a file: %2")
                .arg(documentNumber)
                .arg(cleanInputPath)
        );
    }

    try {
        QPDF inputPdf;
        const QByteArray inputFileName = toQpdfFileName(cleanInputPath);
        inputPdf.processFile(inputFileName.constData());

        QPDFPageDocumentHelper inputPages(inputPdf);
        const std::vector<QPDFPageObjectHelper> pages = inputPages.getAllPages();

        const int pageCount = static_cast<int>(pages.size());

        if (pageCount <= 0) {
            return OperationResult::failure(
                QString("Document %1 has no readable pages: %2")
                    .arg(documentNumber)
                    .arg(cleanInputPath)
            );
        }

        for (int rangeIndex = 0; rangeIndex < ranges.size(); ++rangeIndex) {
            const PageRange& range = ranges.at(rangeIndex);

            OperationResult rangeResult = validateExtractRange(
                range,
                pageCount,
                documentNumber,
                rangeIndex + 1
            );

            if (!rangeResult.ok) {
                return rangeResult;
            }

            QString outputFilePath;
            OperationResult outputPathResult = makeExtractOutputPath(
                cleanInputPath,
                chosenOutputPath,
                rangeIndex + 1,
                ranges.size(),
                outputFilePath
            );

            if (!outputPathResult.ok) {
                return outputPathResult;
            }

            const QString outputKey = QFileInfo(outputFilePath).absoluteFilePath();

            if (plannedOutputPaths.contains(outputKey)) {
                return OperationResult::failure(
                    QString("Output filename collision: %1. This can happen when multiple input PDFs have the same base filename.")
                        .arg(outputFilePath)
                );
            }

            plannedOutputPaths.insert(outputKey);

            QPDF outputPdf;
            outputPdf.emptyPDF();
            QPDFPageDocumentHelper outputPages(outputPdf);

            OperationResult appendResult = appendRangeToOutputPdf(pages, range, outputPages);

            if (!appendResult.ok) {
                return appendResult;
            }

            const QByteArray outputFileName = toQpdfFileName(outputFilePath);
            QPDFWriter writer(outputPdf, outputFileName.constData());
            writer.write();

            createdFiles.append(outputFilePath);
        }

        return OperationResult::success();
    }
    catch (const std::exception& e) {
        return OperationResult::failure(
            QString("qpdf extraction failed for document %1 '%2': %3")
                .arg(documentNumber)
                .arg(cleanInputPath)
                .arg(QString::fromUtf8(e.what()))
        );
    }
}

OperationResult writeSelectedPagesToSinglePdf(
    const QStringList& pdfPaths,
    const QVector<DocumentPageSelection>& selections,
    const QString& outputPath)
{
    if (pdfPaths.isEmpty()) {
        return OperationResult::failure("No PDF files were provided.");
    }

    if (pdfPaths.size() != selections.size()) {
        return OperationResult::failure(
            QString("Internal error: there are %1 PDF file(s), but %2 page selection(s).")
                .arg(pdfPaths.size())
                .arg(selections.size())
        );
    }

    if (outputPath.trimmed().isEmpty()) {
        return OperationResult::failure("No output path was provided.");
    }

    try {
        QPDF outputPdf;
        outputPdf.emptyPDF();
        QPDFPageDocumentHelper outputPages(outputPdf);

        for (int docIndex = 0; docIndex < pdfPaths.size(); ++docIndex) {
            const QString inputPath = pdfPaths.at(docIndex).trimmed();
            const DocumentPageSelection& selection = selections.at(docIndex);
            const QFileInfo inputInfo(inputPath);

            if (!inputInfo.exists()) {
                return OperationResult::failure(
                    QString("Document %1 does not exist: %2")
                        .arg(docIndex + 1)
                        .arg(inputPath)
                );
            }

            if (!inputInfo.isFile()) {
                return OperationResult::failure(
                    QString("Document %1 path is not a file: %2")
                        .arg(docIndex + 1)
                        .arg(inputPath)
                );
            }

            QPDF inputPdf;
            const QByteArray inputFileName = toQpdfFileName(inputPath);
            inputPdf.processFile(inputFileName.constData());

            QPDFPageDocumentHelper inputPages(inputPdf);
            const std::vector<QPDFPageObjectHelper> pages = inputPages.getAllPages();

            if (pages.empty()) {
                return OperationResult::failure(
                    QString("Document %1 has no readable pages: %2")
                        .arg(docIndex + 1)
                        .arg(inputPath)
                );
            }

            QVector<PageRange> ranges = rangesFromSelection(
                selection,
                static_cast<int>(pages.size())
            );

            if (ranges.isEmpty()) {
                return OperationResult::failure(
                    QString("Document %1 has no extraction ranges.").arg(docIndex + 1)
                );
            }

            for (int rangeIndex = 0; rangeIndex < ranges.size(); ++rangeIndex) {
                OperationResult validationResult = validateExtractRange(
                    ranges.at(rangeIndex),
                    static_cast<int>(pages.size()),
                    docIndex + 1,
                    rangeIndex + 1
                );

                if (!validationResult.ok) {
                    return validationResult;
                }

                OperationResult appendResult = appendRangeToOutputPdf(
                    pages,
                    ranges.at(rangeIndex),
                    outputPages
                );

                if (!appendResult.ok) {
                    return appendResult;
                }
            }
        }

        const QString finalOutputPath = ensurePdfExtension(outputPath);
        const QByteArray outputFileName = toQpdfFileName(finalOutputPath);
        QPDFWriter writer(outputPdf, outputFileName.constData());
        writer.write();

        return OperationResult::success();
    }
    catch (const std::exception& e) {
        return OperationResult::failure(
            QString("qpdf extraction failed: %1").arg(QString::fromUtf8(e.what()))
        );
    }
}

OperationResult extractOnePdfPerInputFile(
    const QStringList& pdfPaths,
    const QVector<DocumentPageSelection>& selections,
    const QString& chosenOutputPath,
    QStringList& createdFiles)
{
    if (pdfPaths.size() != selections.size()) {
        return OperationResult::failure(
            QString("Internal error: there are %1 PDF file(s), but %2 page selection(s).")
                .arg(pdfPaths.size())
                .arg(selections.size())
        );
    }

    QSet<QString> plannedOutputPaths;

    for (int docIndex = 0; docIndex < pdfPaths.size(); ++docIndex) {
        QString outputFilePath;
        OperationResult outputPathResult = makeExtractOutputPath(
            pdfPaths.at(docIndex),
            chosenOutputPath,
            docIndex + 1,
            pdfPaths.size(),
            outputFilePath
        );

        if (!outputPathResult.ok) {
            createdFiles.clear();
            return outputPathResult;
        }

        const QString outputKey = QFileInfo(outputFilePath).absoluteFilePath();

        if (plannedOutputPaths.contains(outputKey)) {
            createdFiles.clear();
            return OperationResult::failure(
                QString("Output filename collision: %1").arg(outputFilePath)
            );
        }

        plannedOutputPaths.insert(outputKey);

        QStringList onePdfPath;
        onePdfPath.append(pdfPaths.at(docIndex));

        QVector<DocumentPageSelection> oneSelection;
        oneSelection.append(selections.at(docIndex));

        OperationResult writeResult = writeSelectedPagesToSinglePdf(
            onePdfPath,
            oneSelection,
            outputFilePath
        );

        if (!writeResult.ok) {
            createdFiles.clear();
            return OperationResult::failure(
                QString("Document %1: %2")
                    .arg(docIndex + 1)
                    .arg(writeResult.message)
            );
        }

        createdFiles.append(outputFilePath);
    }

    return OperationResult::success();
}
} // namespace

OperationResult extractPdfs(
    const ExtractOptions& options,
    QStringList* createdFiles)
{
    QStringList localCreatedFiles;

    if (options.inputPaths.isEmpty()) {
        return OperationResult::failure("No PDF files were provided.");
    }

    if (options.outputPath.trimmed().isEmpty()) {
        return OperationResult::failure("No output path was provided.");
    }

    QVector<int> pageCounts;
    OperationResult pageCountResult = getPdfPageCounts(options.inputPaths, pageCounts);

    if (!pageCountResult.ok) {
        return pageCountResult;
    }

    QString parseError;
    QVector<DocumentPageSelection> selections = parsePageSelections(
        options.pageRangeSpec,
        options.inputPaths.size(),
        parseError
    );

    if (!parseError.isEmpty()) {
        return OperationResult::failure(parseError);
    }

    QString validationError;

    if (!validateSelectionsAgainstPageCounts(selections, pageCounts, validationError)) {
        return OperationResult::failure(validationError);
    }

    OperationResult result;

    if (options.outputMode == ExtractOptions::OutputMode::OnePdfPerRange) {
        QVector<QVector<PageRange>> extractRangesByDocument;
        result = resolveExtractRangesFromSelections(
            selections,
            pageCounts,
            extractRangesByDocument
        );

        if (!result.ok) {
            return result;
        }

        QSet<QString> plannedOutputPaths;

        for (int docIndex = 0; docIndex < options.inputPaths.size(); ++docIndex) {
            result = extractOnePdfPerRange(
                options.inputPaths.at(docIndex),
                extractRangesByDocument.at(docIndex),
                options.outputPath,
                docIndex + 1,
                localCreatedFiles,
                plannedOutputPaths
            );

            if (!result.ok) {
                return result;
            }
        }
    }
    else if (options.outputMode == ExtractOptions::OutputMode::OnePdfPerInputFile) {
        result = extractOnePdfPerInputFile(
            options.inputPaths,
            selections,
            options.outputPath,
            localCreatedFiles
        );

        if (!result.ok) {
            return result;
        }
    }
    else {
        const QString finalOutputPath = ensurePdfExtension(options.outputPath);
        result = writeSelectedPagesToSinglePdf(
            options.inputPaths,
            selections,
            finalOutputPath
        );

        if (!result.ok) {
            return result;
        }

        localCreatedFiles.append(finalOutputPath);
    }

    if (createdFiles) {
        *createdFiles = localCreatedFiles;
    }

    return OperationResult::success(
        QString("Created %1 PDF file(s).").arg(localCreatedFiles.size())
    );
}
