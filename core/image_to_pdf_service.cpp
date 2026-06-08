#include "image_to_pdf_service.h"

#include "qpdf_utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QImageReader>
#include <QObject>
#include <QPageSize>
#include <QPainter>
#include <QPagedPaintDevice>
#include <QPdfWriter>
#include <QRectF>
#include <QSizeF>
#include <QTemporaryFile>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <algorithm>
#include <cmath>
#include <exception>
#include <vector>

namespace
{
constexpr qreal MillimetresPerInch = 25.4;
constexpr qreal PdfPointsPerInch = 72.0;
constexpr qreal A4HeightPoints = 842.0;

struct TocLinkAnnotation
{
    int tocPageIndex = 0;
    int targetPageIndex = 0;
    QRectF rectPoints;
};

struct ImagePdfEntry
{
    QString imagePath;
    QString title;
    int targetPageIndex = 0;
};

QVector<ImagePdfEntry> buildImageEntries(const QStringList& imagePaths)
{
    QVector<ImagePdfEntry> entries;
    entries.reserve(imagePaths.size());

    for (const QString& imagePath : imagePaths) {
        QFileInfo fileInfo(imagePath);

        ImagePdfEntry entry;
        entry.imagePath = fileInfo.absoluteFilePath();
        entry.title = fileInfo.fileName();

        entries.append(entry);
    }

    return entries;
}

QRectF deviceRectToPdfPoints(
    const QRectF& deviceRect,
    qreal deviceToPointScale,
    qreal pageHeightPoints)
{
    const qreal x1 = deviceRect.left() * deviceToPointScale;
    const qreal x2 = deviceRect.right() * deviceToPointScale;
    const qreal y1 = pageHeightPoints - (deviceRect.bottom() * deviceToPointScale);
    const qreal y2 = pageHeightPoints - (deviceRect.top() * deviceToPointScale);

    return QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized();
}

QRectF imageTargetRect(
    const QSize& imageSize,
    const QRectF& contentRect,
    const ConvertImgOptions& options)
{
    if (!imageSize.isValid() || imageSize.width() <= 0 || imageSize.height() <= 0) {
        return contentRect;
    }

    if (!options.preserveAspectRatio || options.scaleMode == ImageScaleMode::Stretch) {
        return contentRect;
    }

    QSizeF scaledSize(imageSize);

    if (options.scaleMode == ImageScaleMode::FillPageCrop) {
        scaledSize.scale(contentRect.size(), Qt::KeepAspectRatioByExpanding);
    } else {
        scaledSize.scale(contentRect.size(), Qt::KeepAspectRatio);
    }

    qreal x = contentRect.left();
    qreal y = contentRect.top();

    if (options.centreImageOnPage) {
        x += (contentRect.width() - scaledSize.width()) / 2.0;
        y += (contentRect.height() - scaledSize.height()) / 2.0;
    }

    return QRectF(QPointF(x, y), scaledSize);
}

int calculateRowsPerTocPage(
    QPainter& painter,
    const QRectF& contentRect,
    qreal pointsToDeviceScale)
{
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(16.0);

    QFont entryFont = painter.font();
    entryFont.setPointSizeF(10.0);

    const QFontMetricsF titleMetrics(titleFont, painter.device());
    const QFontMetricsF entryMetrics(entryFont, painter.device());

    const qreal titleBlockHeight = titleMetrics.height() + (34.0 * pointsToDeviceScale);
    const qreal rowHeight = entryMetrics.height() + (10.0 * pointsToDeviceScale);
    const qreal availableHeight = contentRect.height() - titleBlockHeight;

    return std::max(1, static_cast<int>(std::floor(availableHeight / rowHeight)));
}

int calculateTocPageCount(int entryCount, int rowsPerTocPage)
{
    if (entryCount <= 0) {
        return 0;
    }

    return std::max(
        1,
        static_cast<int>(std::ceil(static_cast<double>(entryCount) / rowsPerTocPage))
    );
}

void drawTocPage(
    QPainter& painter,
    const QVector<ImagePdfEntry>& entries,
    int tocPageIndex,
    int tocPageCount,
    int rowsPerTocPage,
    const QRectF& pageRect,
    const QRectF& contentRect,
    qreal deviceToPointScale,
    qreal pageHeightPoints,
    QVector<TocLinkAnnotation>& linkAnnotations)
{
    painter.fillRect(pageRect, Qt::white);

    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(16.0);

    QFont entryFont = painter.font();
    entryFont.setPointSizeF(10.0);

    painter.setPen(Qt::black);
    painter.setFont(titleFont);

    painter.drawText(
        contentRect,
        Qt::AlignLeft | Qt::AlignTop,
        QObject::tr("Table of contents")
    );

    const QFontMetricsF titleMetrics(titleFont, painter.device());
    const QFontMetricsF entryMetrics(entryFont, painter.device());
    const qreal pointsToDeviceScale = 1.0 / deviceToPointScale;

    qreal y = contentRect.top() + titleMetrics.height() + (24.0 * pointsToDeviceScale);

    if (tocPageCount > 1) {
        painter.setFont(entryFont);
        painter.drawText(
            QRectF(contentRect.left(), y - entryMetrics.height(), contentRect.width(), entryMetrics.height() + 4.0),
            Qt::AlignRight | Qt::AlignVCenter,
            QObject::tr("TOC page %1 of %2").arg(tocPageIndex + 1).arg(tocPageCount)
        );
    }

    const int firstEntry = tocPageIndex * rowsPerTocPage;
    // const int lastEntryExclusive = std::min(firstEntry + rowsPerTocPage, entries.size());
    const int lastEntryExclusive =
        std::min(firstEntry + rowsPerTocPage, static_cast<int>(entries.size()));
    const qreal rowHeight = entryMetrics.height() + (10.0 * pointsToDeviceScale);
    const qreal pageNumberColumnWidth = 52.0 * pointsToDeviceScale;
    const qreal gap = 8.0 * pointsToDeviceScale;

    painter.setFont(entryFont);

    for (int i = firstEntry; i < lastEntryExclusive; ++i) {
        const ImagePdfEntry& entry = entries.at(i);

        const QRectF rowRect(
            contentRect.left(),
            y,
            contentRect.width(),
            rowHeight
        );

        const QRectF labelRect(
            rowRect.left(),
            rowRect.top(),
            rowRect.width() - pageNumberColumnWidth - gap,
            rowRect.height()
        );

        const QRectF pageNumberRect(
            rowRect.right() - pageNumberColumnWidth,
            rowRect.top(),
            pageNumberColumnWidth,
            rowRect.height()
        );

        const QString label = QString("%1. %2").arg(i + 1).arg(entry.title);
        const QString elidedLabel = entryMetrics.elidedText(
            label,
            Qt::ElideMiddle,
            static_cast<int>(labelRect.width())
        );
        const QString pageNumber = QString::number(entry.targetPageIndex + 1);

        painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, elidedLabel);
        painter.drawText(pageNumberRect, Qt::AlignRight | Qt::AlignVCenter, pageNumber);

        QPen oldPen = painter.pen();
        QPen dottedPen(Qt::gray);
        dottedPen.setStyle(Qt::DotLine);
        painter.setPen(dottedPen);
        painter.drawLine(
            QPointF(labelRect.right() + (4.0 * pointsToDeviceScale), rowRect.center().y()),
            QPointF(pageNumberRect.left() - (4.0 * pointsToDeviceScale), rowRect.center().y())
        );
        painter.setPen(oldPen);

        TocLinkAnnotation annotation;
        annotation.tocPageIndex = tocPageIndex;
        annotation.targetPageIndex = entry.targetPageIndex;
        annotation.rectPoints = deviceRectToPdfPoints(
            rowRect.adjusted(0.0, 1.0 * pointsToDeviceScale, 0.0, -1.0 * pointsToDeviceScale),
            deviceToPointScale,
            pageHeightPoints
        );

        linkAnnotations.append(annotation);
        y += rowHeight;
    }
}

bool drawImagePage(
    QPainter& painter,
    const ImagePdfEntry& entry,
    const QRectF& pageRect,
    const QRectF& contentRect,
    const ConvertImgOptions& options,
    QString& error)
{
    error.clear();

    QImageReader reader(entry.imagePath);
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    reader.setAutoTransform(options.autoRotateFromMetadata);
#endif

    // const QImage image = reader.read();
    QImage image = reader.read();
    if (image.isNull()) {
        error = QObject::tr("Could not read image '%1': %2")
            .arg(entry.imagePath, reader.errorString());
        return false;
    }

    painter.fillRect(pageRect, options.backgroundColour);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF targetRect = imageTargetRect(image.size(), contentRect, options);

    if (options.scaleMode == ImageScaleMode::FillPageCrop && options.preserveAspectRatio) {
        painter.save();
        painter.setClipRect(contentRect);
        painter.drawImage(targetRect, image);
        painter.restore();
    } else {
        painter.drawImage(targetRect, image);
    }

    return true;
}

bool renderPdfWithQt(
    const ConvertImgOptions& options,
    const QString& outputPdfPath,
    QVector<TocLinkAnnotation>& linkAnnotations,
    QString& error)
{
    error.clear();
    linkAnnotations.clear();

    QVector<ImagePdfEntry> entries = buildImageEntries(options.imagePaths);

    if (entries.isEmpty()) {
        error = QObject::tr("No images were provided.");
        return false;
    }

    QPdfWriter writer(outputPdfPath);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    writer.setPageSize(QPageSize(QPageSize::A4));
#else
    writer.setPageSize(QPagedPaintDevice::A4);
#endif
    writer.setResolution(options.pdfResolutionDpi);
    writer.setCreator(QStringLiteral("PDFutils"));

    QPainter painter;

    if (!painter.begin(&writer)) {
        error = QObject::tr("Could not start writing PDF: %1").arg(outputPdfPath);
        return false;
    }

    const qreal dpi = static_cast<qreal>(options.pdfResolutionDpi);
    const qreal deviceToPointScale = PdfPointsPerInch / dpi;
    const qreal pointsToDeviceScale = 1.0 / deviceToPointScale;
    const qreal marginDevice = (static_cast<qreal>(options.marginMillimetres) / MillimetresPerInch) * dpi;

    const QRectF pageRect(0.0, 0.0, writer.width(), writer.height());
    const QRectF contentRect = pageRect.adjusted(
        marginDevice,
        marginDevice,
        -marginDevice,
        -marginDevice
    );

    const qreal pageHeightPoints = A4HeightPoints;

    int rowsPerTocPage = 0;
    int tocPageCount = 0;

    if (options.generateHyperlinkedTableOfContents) {
        rowsPerTocPage = calculateRowsPerTocPage(painter, contentRect, pointsToDeviceScale);
        tocPageCount = calculateTocPageCount(entries.size(), rowsPerTocPage);

        for (int i = 0; i < entries.size(); ++i) {
            entries[i].targetPageIndex = tocPageCount + i;
        }
    } else {
        for (int i = 0; i < entries.size(); ++i) {
            entries[i].targetPageIndex = i;
        }
    }

    bool firstPage = true;

    auto beginNextPage = [&]() -> bool {
        if (firstPage) {
            firstPage = false;
            return true;
        }

        if (!writer.newPage()) {
            error = QObject::tr("Could not create a new PDF page.");
            return false;
        }

        return true;
    };

    if (options.generateHyperlinkedTableOfContents) {
        for (int tocPageIndex = 0; tocPageIndex < tocPageCount; ++tocPageIndex) {
            if (!beginNextPage()) {
                painter.end();
                return false;
            }

            drawTocPage(
                painter,
                entries,
                tocPageIndex,
                tocPageCount,
                rowsPerTocPage,
                pageRect,
                contentRect,
                deviceToPointScale,
                pageHeightPoints,
                linkAnnotations
            );
        }
    }

    for (const ImagePdfEntry& entry : entries) {
        if (!beginNextPage()) {
            painter.end();
            return false;
        }

        if (!drawImagePage(painter, entry, pageRect, contentRect, options, error)) {
            painter.end();
            return false;
        }
    }

    painter.end();
    return true;
}

QPDFObjectHandle newPdfReal(qreal value)
{
    return QPDFObjectHandle::newReal(QString::number(value, 'f', 2).toStdString());
}

QPDFObjectHandle newPdfRectArray(const QRectF& rect)
{
    QPDFObjectHandle array = QPDFObjectHandle::newArray();

    array.appendItem(newPdfReal(rect.left()));
    array.appendItem(newPdfReal(rect.top()));
    array.appendItem(newPdfReal(rect.right()));
    array.appendItem(newPdfReal(rect.bottom()));

    return array;
}

QPDFObjectHandle newPdfBorderArray()
{
    QPDFObjectHandle array = QPDFObjectHandle::newArray();

    array.appendItem(QPDFObjectHandle::newInteger(0));
    array.appendItem(QPDFObjectHandle::newInteger(0));
    array.appendItem(QPDFObjectHandle::newInteger(0));

    return array;
}

QPDFObjectHandle newPdfDestinationArray(const QPDFObjectHandle& targetPage)
{
    QPDFObjectHandle destination = QPDFObjectHandle::newArray();

    destination.appendItem(targetPage);
    destination.appendItem(QPDFObjectHandle::newName("/Fit"));

    return destination;
}

bool addTocLinkAnnotationsWithQpdf(
    const QString& inputPdfPath,
    const QString& outputPdfPath,
    const QVector<TocLinkAnnotation>& linkAnnotations,
    QString& error)
{
    error.clear();

    if (linkAnnotations.isEmpty()) {
        error = QObject::tr("No table-of-contents link annotations were generated.");
        return false;
    }

    try {
        QPDF pdf;
        const QByteArray inputFileName = toQpdfFileName(inputPdfPath);
        pdf.processFile(inputFileName.constData());

        QPDFPageDocumentHelper pageHelper(pdf);
        std::vector<QPDFPageObjectHelper> pageHelpers = pageHelper.getAllPages();

        if (pageHelpers.empty()) {
            error = QObject::tr("The generated temporary PDF has no pages.");
            return false;
        }

        for (const TocLinkAnnotation& link : linkAnnotations) {
            if (link.tocPageIndex < 0 || link.tocPageIndex >= static_cast<int>(pageHelpers.size())) {
                error = QObject::tr("Internal error: invalid TOC page index %1.").arg(link.tocPageIndex + 1);
                return false;
            }

            if (link.targetPageIndex < 0 || link.targetPageIndex >= static_cast<int>(pageHelpers.size())) {
                error = QObject::tr("Internal error: invalid TOC target page index %1.").arg(link.targetPageIndex + 1);
                return false;
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
            annotation.replaceKey("/Rect", newPdfRectArray(link.rectPoints));
            annotation.replaceKey("/Border", newPdfBorderArray());
            annotation.replaceKey("/Dest", newPdfDestinationArray(targetPage));

            annots.appendItem(pdf.makeIndirectObject(annotation));
        }

        const QByteArray outputFileName = toQpdfFileName(outputPdfPath);
        QPDFWriter writer(pdf, outputFileName.constData());
        writer.write();

        return true;
    }
    catch (const std::exception& e) {
        error = QObject::tr("Could not add hyperlinked table of contents: %1")
            .arg(QString::fromUtf8(e.what()));
        return false;
    }
}

OperationResult validateOptions(const ConvertImgOptions& options)
{
    if (options.imagePaths.isEmpty()) {
        return OperationResult::failure(QObject::tr("No images were selected."));
    }

    if (options.outputPdfPath.trimmed().isEmpty()) {
        return OperationResult::failure(QObject::tr("No output PDF path was provided."));
    }

    if (!options.oneImagePerPage) {
        return OperationResult::failure(
            QObject::tr("The current converter supports only one image per PDF page. Enable the one-image-per-page option.")
        );
    }

    if (options.pdfResolutionDpi <= 0) {
        return OperationResult::failure(QObject::tr("PDF resolution must be greater than zero."));
    }

    if (options.marginMillimetres < 0) {
        return OperationResult::failure(QObject::tr("PDF margin cannot be negative."));
    }

    for (int i = 0; i < options.imagePaths.size(); ++i) {
        const QFileInfo fileInfo(options.imagePaths.at(i).trimmed());

        if (!fileInfo.exists() || !fileInfo.isFile()) {
            return OperationResult::failure(
                QObject::tr("Image %1 is not a readable file: %2")
                    .arg(i + 1)
                    .arg(options.imagePaths.at(i))
            );
        }

        QImageReader reader(fileInfo.absoluteFilePath());
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
        reader.setAutoTransform(options.autoRotateFromMetadata);
#endif

        if (!reader.canRead()) {
            return OperationResult::failure(
                QObject::tr("Image %1 is not supported or cannot be read: %2")
                    .arg(i + 1)
                    .arg(fileInfo.absoluteFilePath())
            );
        }
    }

    return OperationResult::success();
}

} // namespace

QString normaliseImagePdfOutputPath(const QString& outputPath)
{
    return ensurePdfExtension(outputPath);
}

OperationResult convertImagesToPdf(const ConvertImgOptions& options)
{
    const OperationResult validation = validateOptions(options);

    if (!validation.ok) {
        return validation;
    }

    const QString finalOutputPath = normaliseImagePdfOutputPath(options.outputPdfPath);

    const QFileInfo outputInfo(finalOutputPath);
    const QDir outputDir = outputInfo.absoluteDir();

    if (!outputDir.exists()) {
        if (!QDir().mkpath(outputDir.absolutePath())) {
            return OperationResult::failure(
                QObject::tr("Could not create output directory: %1")
                    .arg(outputDir.absolutePath())
            );
        }
    }

    if (!options.generateHyperlinkedTableOfContents) {
        QVector<TocLinkAnnotation> unusedLinks;
        QString error;

        if (!renderPdfWithQt(options, finalOutputPath, unusedLinks, error)) {
            QFile::remove(finalOutputPath);
            return OperationResult::failure(error);
        }

        return OperationResult::success(
            QObject::tr("The image PDF was created successfully: %1")
                .arg(finalOutputPath)
        );
    }

    QTemporaryFile temporaryFile(QDir::tempPath() + "/PDFutils_convertimg_XXXXXX.pdf");
    temporaryFile.setAutoRemove(false);

    if (!temporaryFile.open()) {
        return OperationResult::failure(QObject::tr("Could not create a temporary PDF file."));
    }

    const QString temporaryPdfPath = temporaryFile.fileName();
    temporaryFile.close();
    QFile::remove(temporaryPdfPath);

    QVector<TocLinkAnnotation> linkAnnotations;
    QString error;

    if (!renderPdfWithQt(options, temporaryPdfPath, linkAnnotations, error)) {
        QFile::remove(temporaryPdfPath);
        return OperationResult::failure(error);
    }

    if (!addTocLinkAnnotationsWithQpdf(temporaryPdfPath, finalOutputPath, linkAnnotations, error)) {
        QFile::remove(temporaryPdfPath);
        QFile::remove(finalOutputPath);
        return OperationResult::failure(error);
    }

    QFile::remove(temporaryPdfPath);

    return OperationResult::success(
        QObject::tr("The image PDF was created successfully with a hyperlinked table of contents: %1")
            .arg(finalOutputPath)
    );
}
