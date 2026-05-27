#include "merge_service.h"

#include "page_range.h"
#include "qpdf_utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QPageSize>
#include <QPainter>
#include <QPagedPaintDevice>
#include <QPdfWriter>
#include <QRectF>
#include <QTemporaryFile>
#include <QVector>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <algorithm>
#include <exception>
#include <vector>

namespace
{
struct MergeTocEntry
{
    QString title;
    QString selectedPagesText;
    int targetPageIndex = 0;
    QRectF rowRectPoints;
};

struct MergeTocLinkAnnotation
{
    int tocPageIndex = 0;
    int targetPageIndex = 0;
    QRectF rectPoints;
};

OperationResult validateMergeOptionsBasics(const MergeOptions& options)
{
    if (options.inputPaths.isEmpty()) {
        return OperationResult::failure("No PDF files were provided.");
    }

    if (options.outputPath.trimmed().isEmpty()) {
        return OperationResult::failure("No output file was provided.");
    }

    return OperationResult::success();
}

OperationResult buildSelectionsAndPageCounts(
    const MergeOptions& options,
    QVector<DocumentPageSelection>& selections,
    QVector<int>& pageCounts)
{
    QString parseError;
    selections = parsePageSelections(
        options.pageRangeSpec,
        options.inputPaths.size(),
        parseError
    );

    if (!parseError.isEmpty()) {
        return OperationResult::failure(parseError);
    }

    OperationResult pageCountResult = getPdfPageCounts(options.inputPaths, pageCounts);

    if (!pageCountResult.ok) {
        return pageCountResult;
    }

    QString validationError;

    if (!validateSelectionsAgainstPageCounts(selections, pageCounts, validationError)) {
        return OperationResult::failure(validationError);
    }

    return OperationResult::success();
}

OperationResult mergeSelectedPagesWithQpdf(
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
        return OperationResult::failure("No output file was provided.");
    }

    const QString finalOutputPath = ensurePdfExtension(outputPath);

    try {
        QPDF outputPdf;
        outputPdf.emptyPDF();

        QPDFPageDocumentHelper outputPages(outputPdf);

        for (int docIndex = 0; docIndex < pdfPaths.size(); ++docIndex) {
            const QString inputPath = pdfPaths[docIndex].trimmed();
            const DocumentPageSelection& selection = selections[docIndex];

            QFileInfo inputInfo(inputPath);

            if (!inputInfo.exists()) {
                return OperationResult::failure(
                    QString("Document %1 does not exist: %2")
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

            if (selection.allPages) {
                for (const QPDFPageObjectHelper& page : pages) {
                    outputPages.addPage(page, false);
                }

                continue;
            }

            for (const PageRange& range : selection.ranges) {
                for (int pageNumber = range.first; pageNumber <= range.last; ++pageNumber) {
                    const int pageIndex = pageNumber - 1;

                    if (pageIndex < 0 || pageIndex >= static_cast<int>(pages.size())) {
                        return OperationResult::failure(
                            QString("Document %1 has %2 page(s), but page %3 was requested.")
                                .arg(docIndex + 1)
                                .arg(static_cast<int>(pages.size()))
                                .arg(pageNumber)
                        );
                    }

                    outputPages.addPage(pages.at(static_cast<size_t>(pageIndex)), false);
                }
            }
        }

        const QByteArray outputFileName = toQpdfFileName(finalOutputPath);
        QPDFWriter writer(outputPdf, outputFileName.constData());
        writer.write();

        return OperationResult::success(
            QString("The PDF files were merged successfully: %1").arg(finalOutputPath)
        );
    }
    catch (const std::exception& e) {
        return OperationResult::failure(
            QString("qpdf merge failed: %1").arg(QString::fromUtf8(e.what()))
        );
    }
}

bool buildMergeTocEntries(
    const QStringList& pdfPaths,
    const QVector<DocumentPageSelection>& selections,
    const QVector<int>& pageCounts,
    QVector<MergeTocEntry>& entries,
    QString& error)
{
    entries.clear();
    error.clear();

    if (pdfPaths.isEmpty()) {
        error = "No PDF files were provided.";
        return false;
    }

    if (pdfPaths.size() != selections.size() || pdfPaths.size() != pageCounts.size()) {
        error = "Internal error: inconsistent merge input counts while building the table of contents.";
        return false;
    }

    int nextOutputPageIndex = 1;

    for (int i = 0; i < pdfPaths.size(); ++i) {
        const int selectedCount = selectedPageCountForDocument(selections[i], pageCounts[i]);

        if (selectedCount <= 0) {
            error = QString("Document %1 contributes no pages to the merged PDF.").arg(i + 1);
            return false;
        }

        QFileInfo fileInfo(pdfPaths[i]);

        MergeTocEntry entry;
        entry.title = fileInfo.fileName();
        entry.selectedPagesText = selectedPagesTextForDocument(selections[i]);
        entry.targetPageIndex = nextOutputPageIndex;

        entries.append(entry);
        nextOutputPageIndex += selectedCount;
    }

    return true;
}

QRectF topLeftDeviceRectToPdfRect(
    const QRectF& deviceRect,
    qreal pageHeightPoints)
{
    const qreal x1 = deviceRect.left();
    const qreal x2 = deviceRect.right();
    const qreal y1 = pageHeightPoints - deviceRect.bottom();
    const qreal y2 = pageHeightPoints - deviceRect.top();

    return QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized();
}

bool renderMergeTocPdfWithQt(
    const QString& tocPdfPath,
    QVector<MergeTocEntry>& entries,
    QVector<MergeTocLinkAnnotation>& linkAnnotations,
    QString& error)
{
    error.clear();
    linkAnnotations.clear();

    if (entries.isEmpty()) {
        error = "No table-of-contents entries were generated.";
        return false;
    }

    QPdfWriter writer(tocPdfPath);
    writer.setResolution(72);
    writer.setCreator("PDFutils");
#if QT_VERSION >= QT_VERSION_CHECK(5, 3, 0)
    writer.setPageSize(QPageSize(QPageSize::A4));
#else
    writer.setPageSize(QPagedPaintDevice::A4);
#endif

    QPainter painter;

    if (!painter.begin(&writer)) {
        error = "Could not render the table-of-contents PDF page.";
        return false;
    }

    const QRectF pageRect(0.0, 0.0, writer.width(), writer.height());
    const qreal margin = 50.0;
    const QRectF contentRect = pageRect.adjusted(margin, margin, -margin, -margin);
    const qreal pageHeightPoints = pageRect.height();

    QFont titleFont = painter.font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);

    QFont rowFont = painter.font();
    rowFont.setPointSize(10);

    painter.setFont(titleFont);
    painter.drawText(
        QRectF(contentRect.left(), contentRect.top(), contentRect.width(), 30.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        "Table of contents"
    );

    painter.setFont(rowFont);
    QFontMetricsF metrics(rowFont);

    const qreal rowHeight = 24.0;
    qreal y = contentRect.top() + 48.0;

    const qreal numberWidth = 36.0;
    const qreal pageNumberWidth = 52.0;
    const qreal gap = 10.0;

    for (int i = 0; i < entries.size(); ++i) {
        if (y + rowHeight > contentRect.bottom()) {
            painter.end();
            error = "The table of contents does not fit on one page. Reduce the number of input PDFs or implement multi-page TOC support.";
            return false;
        }

        MergeTocEntry& entry = entries[i];

        const QRectF rowRect(
            contentRect.left(),
            y,
            contentRect.width(),
            rowHeight
        );

        const QRectF numberRect(
            rowRect.left(),
            rowRect.top(),
            numberWidth,
            rowRect.height()
        );

        const QRectF pageNumberRect(
            rowRect.right() - pageNumberWidth,
            rowRect.top(),
            pageNumberWidth,
            rowRect.height()
        );

        const QRectF labelRect(
            numberRect.right() + gap,
            rowRect.top(),
            rowRect.width() - numberWidth - pageNumberWidth - (2.0 * gap),
            rowRect.height()
        );

        const QString numberText = QString::number(i + 1) + ".";
        const QString labelText = QString("%1 - %2").arg(entry.title, entry.selectedPagesText);
        const QString pageText = QString::number(entry.targetPageIndex + 1);

        painter.drawText(numberRect, Qt::AlignLeft | Qt::AlignVCenter, numberText);
        painter.drawText(
            labelRect,
            Qt::AlignLeft | Qt::AlignVCenter,
            metrics.elidedText(labelText, Qt::ElideMiddle, static_cast<int>(labelRect.width()))
        );
        painter.drawText(pageNumberRect, Qt::AlignRight | Qt::AlignVCenter, pageText);

        entry.rowRectPoints = topLeftDeviceRectToPdfRect(rowRect, pageHeightPoints);

        MergeTocLinkAnnotation link;
        link.tocPageIndex = 0;
        link.targetPageIndex = entry.targetPageIndex;
        link.rectPoints = entry.rowRectPoints;
        linkAnnotations.append(link);

        y += rowHeight;
    }

    painter.end();
    return true;
}

OperationResult mergeTocAndSelectedPagesWithQpdf(
    const QString& tocPdfPath,
    const QStringList& pdfPaths,
    const QVector<DocumentPageSelection>& selections,
    const QString& temporaryMergedPath)
{
    try {
        QPDF outputPdf;
        outputPdf.emptyPDF();
        QPDFPageDocumentHelper outputPages(outputPdf);

        {
            QPDF tocPdf;
            const QByteArray tocFileName = toQpdfFileName(tocPdfPath);
            tocPdf.processFile(tocFileName.constData());

            QPDFPageDocumentHelper tocPages(tocPdf);
            const std::vector<QPDFPageObjectHelper> pages = tocPages.getAllPages();

            if (pages.empty()) {
                return OperationResult::failure("The generated table-of-contents PDF has no pages.");
            }

            for (const QPDFPageObjectHelper& page : pages) {
                outputPages.addPage(page, false);
            }
        }

        for (int docIndex = 0; docIndex < pdfPaths.size(); ++docIndex) {
            const QString inputPath = pdfPaths[docIndex].trimmed();
            const DocumentPageSelection& selection = selections[docIndex];

            QFileInfo inputInfo(inputPath);

            if (!inputInfo.exists()) {
                return OperationResult::failure(
                    QString("Document %1 does not exist: %2")
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

            if (selection.allPages) {
                for (const QPDFPageObjectHelper& page : pages) {
                    outputPages.addPage(page, false);
                }
                continue;
            }

            for (const PageRange& range : selection.ranges) {
                for (int pageNumber = range.first; pageNumber <= range.last; ++pageNumber) {
                    const int pageIndex = pageNumber - 1;

                    if (pageIndex < 0 || pageIndex >= static_cast<int>(pages.size())) {
                        return OperationResult::failure(
                            QString("Document %1 has %2 page(s), but page %3 was requested.")
                                .arg(docIndex + 1)
                                .arg(static_cast<int>(pages.size()))
                                .arg(pageNumber)
                        );
                    }

                    outputPages.addPage(pages.at(static_cast<size_t>(pageIndex)), false);
                }
            }
        }

        const QByteArray outputFileName = toQpdfFileName(temporaryMergedPath);
        QPDFWriter writer(outputPdf, outputFileName.constData());
        writer.write();

        return OperationResult::success();
    }
    catch (const std::exception& e) {
        return OperationResult::failure(
            QString("qpdf merge with table of contents failed: %1")
                .arg(QString::fromUtf8(e.what()))
        );
    }
}

QPDFObjectHandle newMergePdfReal(qreal value)
{
    return QPDFObjectHandle::newReal(QString::number(value, 'f', 2).toStdString());
}

QPDFObjectHandle newMergePdfRectArray(const QRectF& rect)
{
    QPDFObjectHandle array = QPDFObjectHandle::newArray();

    array.appendItem(newMergePdfReal(rect.left()));
    array.appendItem(newMergePdfReal(rect.top()));
    array.appendItem(newMergePdfReal(rect.right()));
    array.appendItem(newMergePdfReal(rect.bottom()));

    return array;
}

QPDFObjectHandle newMergePdfBorderArray()
{
    QPDFObjectHandle array = QPDFObjectHandle::newArray();

    array.appendItem(QPDFObjectHandle::newInteger(0));
    array.appendItem(QPDFObjectHandle::newInteger(0));
    array.appendItem(QPDFObjectHandle::newInteger(0));

    return array;
}

QPDFObjectHandle newMergePdfDestinationArray(const QPDFObjectHandle& targetPage)
{
    QPDFObjectHandle destination = QPDFObjectHandle::newArray();

    destination.appendItem(targetPage);
    destination.appendItem(QPDFObjectHandle::newName("/Fit"));

    return destination;
}

OperationResult addMergeTocLinkAnnotationsWithQpdf(
    const QString& inputPdfPath,
    const QString& outputPdfPath,
    const QVector<MergeTocLinkAnnotation>& linkAnnotations)
{
    if (linkAnnotations.isEmpty()) {
        return OperationResult::failure("No table-of-contents link annotations were generated.");
    }

    try {
        QPDF pdf;
        const QByteArray inputFileName = toQpdfFileName(inputPdfPath);
        pdf.processFile(inputFileName.constData());

        QPDFPageDocumentHelper pageHelper(pdf);
        std::vector<QPDFPageObjectHelper> pageHelpers = pageHelper.getAllPages();

        if (pageHelpers.empty()) {
            return OperationResult::failure("The generated temporary merged PDF has no pages.");
        }

        for (const MergeTocLinkAnnotation& link : linkAnnotations) {
            if (link.tocPageIndex < 0 || link.tocPageIndex >= static_cast<int>(pageHelpers.size())) {
                return OperationResult::failure(
                    QString("Internal error: invalid TOC page index %1.").arg(link.tocPageIndex + 1)
                );
            }

            if (link.targetPageIndex < 0 || link.targetPageIndex >= static_cast<int>(pageHelpers.size())) {
                return OperationResult::failure(
                    QString("Internal error: invalid TOC target page index %1.").arg(link.targetPageIndex + 1)
                );
            }

            QPDFObjectHandle tocPage = pageHelpers.at(static_cast<size_t>(link.tocPageIndex)).getObjectHandle();
            QPDFObjectHandle targetPage = pageHelpers.at(static_cast<size_t>(link.targetPageIndex)).getObjectHandle();

            QPDFObjectHandle annots = tocPage.getKey("/Annots");

            if (!annots.isArray()) {
                annots = QPDFObjectHandle::newArray();
                tocPage.replaceKey("/Annots", annots);
            }

            QPDFObjectHandle annotation = QPDFObjectHandle::newDictionary();
            annotation.replaceKey("/Type", QPDFObjectHandle::newName("/Annot"));
            annotation.replaceKey("/Subtype", QPDFObjectHandle::newName("/Link"));
            annotation.replaceKey("/Rect", newMergePdfRectArray(link.rectPoints));
            annotation.replaceKey("/Border", newMergePdfBorderArray());
            annotation.replaceKey("/Dest", newMergePdfDestinationArray(targetPage));

            annots.appendItem(pdf.makeIndirectObject(annotation));
        }

        const QByteArray outputFileName = toQpdfFileName(outputPdfPath);
        QPDFWriter writer(pdf, outputFileName.constData());
        writer.write();

        return OperationResult::success();
    }
    catch (const std::exception& e) {
        return OperationResult::failure(
            QString("Could not add hyperlinked table of contents: %1")
                .arg(QString::fromUtf8(e.what()))
        );
    }
}

OperationResult mergeSelectedPagesWithHyperlinkedToc(
    const QStringList& pdfPaths,
    const QVector<DocumentPageSelection>& selections,
    const QVector<int>& pageCounts,
    const QString& outputPath)
{
    const QString finalOutputPath = ensurePdfExtension(outputPath);

    QVector<MergeTocEntry> tocEntries;
    QString tocError;

    if (!buildMergeTocEntries(pdfPaths, selections, pageCounts, tocEntries, tocError)) {
        return OperationResult::failure(tocError);
    }

    QTemporaryFile tocFile(QDir::tempPath() + "/PDFutils_merge_toc_XXXXXX.pdf");
    tocFile.setAutoRemove(false);

    if (!tocFile.open()) {
        return OperationResult::failure("Could not create a temporary table-of-contents PDF file.");
    }

    const QString tocPdfPath = tocFile.fileName();
    tocFile.close();
    QFile::remove(tocPdfPath);

    QTemporaryFile mergedFile(QDir::tempPath() + "/PDFutils_merge_with_toc_XXXXXX.pdf");
    mergedFile.setAutoRemove(false);

    if (!mergedFile.open()) {
        QFile::remove(tocPdfPath);
        return OperationResult::failure("Could not create a temporary merged PDF file.");
    }

    const QString temporaryMergedPath = mergedFile.fileName();
    mergedFile.close();
    QFile::remove(temporaryMergedPath);

    QVector<MergeTocLinkAnnotation> linkAnnotations;
    QString renderError;

    if (!renderMergeTocPdfWithQt(tocPdfPath, tocEntries, linkAnnotations, renderError)) {
        QFile::remove(tocPdfPath);
        QFile::remove(temporaryMergedPath);
        return OperationResult::failure(renderError);
    }

    OperationResult mergeResult = mergeTocAndSelectedPagesWithQpdf(
        tocPdfPath,
        pdfPaths,
        selections,
        temporaryMergedPath
    );

    if (!mergeResult.ok) {
        QFile::remove(tocPdfPath);
        QFile::remove(temporaryMergedPath);
        return mergeResult;
    }

    OperationResult annotationResult = addMergeTocLinkAnnotationsWithQpdf(
        temporaryMergedPath,
        finalOutputPath,
        linkAnnotations
    );

    QFile::remove(tocPdfPath);
    QFile::remove(temporaryMergedPath);

    if (!annotationResult.ok) {
        return annotationResult;
    }

    return OperationResult::success(
        QString("The PDF files were merged successfully with a hyperlinked table of contents: %1")
            .arg(finalOutputPath)
    );
}
} // namespace

OperationResult mergePdfs(const MergeOptions& options)
{
    OperationResult basicValidation = validateMergeOptionsBasics(options);

    if (!basicValidation.ok) {
        return basicValidation;
    }

    QVector<DocumentPageSelection> selections;
    QVector<int> pageCounts;

    OperationResult parseAndValidateResult = buildSelectionsAndPageCounts(
        options,
        selections,
        pageCounts
    );

    if (!parseAndValidateResult.ok) {
        return parseAndValidateResult;
    }

    if (options.generateHyperlinkedToc) {
        return mergeSelectedPagesWithHyperlinkedToc(
            options.inputPaths,
            selections,
            pageCounts,
            options.outputPath
        );
    }

    return mergeSelectedPagesWithQpdf(
        options.inputPaths,
        selections,
        options.outputPath
    );
}
