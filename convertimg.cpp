#include "convertimg.h"
#include "ui_convertimg.h"

#include <QAbstractItemView>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QHeaderView>
#include <QImage>
#include <QImageReader>
#include <QMessageBox>
#include <QPageSize>
#include <QPainter>
#include <QPagedPaintDevice>
#include <QPdfWriter>
#include <QPushButton>
#include <QRectF>
#include <QSet>
#include <QSizeF>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryFile>

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <vector>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

namespace
{

constexpr qreal MillimetresPerInch = 25.4;
constexpr qreal PdfPointsPerInch = 72.0;
constexpr qreal A4HeightPoints = 842.0;

struct TocLinkAnnotation
{
    int tocPageIndex = 0;      // zero-based index in the final PDF
    int targetPageIndex = 0;   // zero-based index in the final PDF
    QRectF rectPoints;         // PDF coordinates, in points, bottom-left origin
};

struct ImagePdfEntry
{
    QString imagePath;
    QString title;
    int targetPageIndex = 0;
};

QString formatFileSize(qint64 bytes)
{
    static const char* units[] = { "B", "KB", "MB", "GB", "TB" };

    double size = static_cast<double>(bytes);
    int unitIndex = 0;

    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        ++unitIndex;
    }

    if (unitIndex == 0) {
        return QString::number(bytes) + " B";
    }

    return QString::number(size, 'f', 2) + ' ' + units[unitIndex];
}

QString formatDimensions(const QSize& dimensions)
{
    if (!dimensions.isValid() || dimensions.width() <= 0 || dimensions.height() <= 0) {
        return QObject::tr("Unknown");
    }

    return QObject::tr("%1 x %2 px")
        .arg(dimensions.width())
        .arg(dimensions.height());
}

QString imageFileDialogFilter()
{
    QStringList patterns;

    const QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();

    for (const QByteArray& format : supportedFormats) {
        const QString suffix = QString::fromLatin1(format).toLower().trimmed();

        if (!suffix.isEmpty()) {
            patterns.append("*." + suffix);
        }
    }

    patterns.removeDuplicates();
    patterns.sort(Qt::CaseInsensitive);

    if (patterns.isEmpty()) {
        return QObject::tr("Image files (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp);;All files (*)");
    }

    return QObject::tr("Image files (%1);;All files (*)").arg(patterns.join(' '));
}

QString ensurePdfExtension(const QString& outputPath)
{
    QString finalOutputPath = outputPath.trimmed();

    if (!finalOutputPath.endsWith(".pdf", Qt::CaseInsensitive)) {
        finalOutputPath += ".pdf";
    }

    return finalOutputPath;
}

QByteArray toQpdfFileName(const QString& path)
{
    QFileInfo fileInfo(path.trimmed());

    const QString absolutePath =
        QDir::toNativeSeparators(fileInfo.absoluteFilePath());

    return absolutePath.toUtf8();
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

bool getImageFileTableRowData(
    const QString& imagePath,
    ImageFileTableRowData& rowData,
    QString& error)
{
    error.clear();

    const QString cleanPath = imagePath.trimmed();

    if (cleanPath.isEmpty()) {
        error = QObject::tr("Empty image path.");
        return false;
    }

    QFileInfo fileInfo(cleanPath);

    if (!fileInfo.exists()) {
        error = QObject::tr("File does not exist: %1").arg(cleanPath);
        return false;
    }

    if (!fileInfo.isFile()) {
        error = QObject::tr("Path is not a file: %1").arg(cleanPath);
        return false;
    }

    QImageReader reader(fileInfo.absoluteFilePath());
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    reader.setAutoTransform(true);
#endif

    const QSize dimensions = reader.size();

    if (!reader.canRead() || !dimensions.isValid()) {
        error = QObject::tr("Unsupported or unreadable image file: %1").arg(fileInfo.absoluteFilePath());
        return false;
    }

    rowData.path = fileInfo.absoluteFilePath();
    rowData.fileSizeBytes = fileInfo.size();
    rowData.dimensions = dimensions;

    return true;
}

void setImageFileTableRow(
    QTableWidget* tableWidget,
    int row,
    const ImageFileTableRowData& rowData,
    const ImageTableColumns& columns = ImageTableColumns())
{
    tableWidget->setItem(
        row,
        columns.path,
        new QTableWidgetItem(rowData.path)
        );

    QTableWidgetItem* sizeItem = new QTableWidgetItem(formatFileSize(rowData.fileSizeBytes));
    sizeItem->setData(Qt::UserRole, rowData.fileSizeBytes);
    sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    tableWidget->setItem(
        row,
        columns.fileSize,
        sizeItem
        );

    QTableWidgetItem* dimensionsItem = new QTableWidgetItem(formatDimensions(rowData.dimensions));
    dimensionsItem->setData(Qt::UserRole, rowData.dimensions);
    dimensionsItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    tableWidget->setItem(
        row,
        columns.dimensions,
        dimensionsItem
        );
}

bool appendImageFileToTableWidget(
    QTableWidget* tableWidget,
    const QString& imagePath,
    QString& error,
    const ImageTableColumns& columns = ImageTableColumns())
{
    if (!tableWidget) {
        error = QObject::tr("Internal error: table widget is null.");
        return false;
    }

    ImageFileTableRowData rowData;

    if (!getImageFileTableRowData(imagePath, rowData, error)) {
        return false;
    }

    const int row = tableWidget->rowCount();
    tableWidget->insertRow(row);

    setImageFileTableRow(tableWidget, row, rowData, columns);

    return true;
}

bool appendImageFilesToTableWidget(
    QTableWidget* tableWidget,
    const QStringList& imagePaths,
    QStringList& errors,
    const ImageTableColumns& columns = ImageTableColumns())
{
    bool allOk = true;

    for (const QString& imagePath : imagePaths) {
        QString error;

        if (!appendImageFileToTableWidget(tableWidget, imagePath, error, columns)) {
            allOk = false;
            errors.append(error);
        }
    }

    return allOk;
}

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
    qDebug() << "[convertimg] Rendering TOC page" << (tocPageIndex + 1) << "of" << tocPageCount;

    painter.fillRect(pageRect, Qt::white);

    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(16.0);

    QFont entryFont = painter.font();
    entryFont.setPointSizeF(10.0);

    painter.setPen(Qt::black);
    painter.setFont(titleFont);

    const QString title = QObject::tr("Table of contents");
    painter.drawText(contentRect, Qt::AlignLeft | Qt::AlignTop, title);

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
    const int lastEntryExclusive = std::min(firstEntry + rowsPerTocPage, entries.size());
    const qreal rowHeight = entryMetrics.height() + (10.0 * pointsToDeviceScale);
    const qreal pageNumberColumnWidth = 52.0 * pointsToDeviceScale;
    const qreal gap = 8.0 * pointsToDeviceScale;

    painter.setFont(entryFont);

    for (int i = firstEntry; i < lastEntryExclusive; ++i) {
        const ImagePdfEntry& entry = entries.at(i);

        QRectF rowRect(
            contentRect.left(),
            y,
            contentRect.width(),
            rowHeight
            );

        QRectF labelRect(
            rowRect.left(),
            rowRect.top(),
            rowRect.width() - pageNumberColumnWidth - gap,
            rowRect.height()
            );

        QRectF pageNumberRect(
            rowRect.right() - pageNumberColumnWidth,
            rowRect.top(),
            pageNumberColumnWidth,
            rowRect.height()
            );

        const QString label = QString("%1. %2").arg(i + 1).arg(entry.title);
        const QString elidedLabel = entryMetrics.elidedText(label, Qt::ElideMiddle, static_cast<int>(labelRect.width()));
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

        qDebug() << "[convertimg] TOC link row" << (i + 1)
                 << "tocPage" << (annotation.tocPageIndex + 1)
                 << "targetPage" << (annotation.targetPageIndex + 1)
                 << "rect" << annotation.rectPoints;

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

    qDebug() << "[convertimg] Rendering image page:" << entry.imagePath;

    QImageReader reader(entry.imagePath);
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    reader.setAutoTransform(options.autoRotateFromMetadata);
#endif

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

    qDebug() << "[convertimg] renderPdfWithQt output:" << outputPdfPath;
    qDebug() << "[convertimg] renderPdfWithQt image count:" << entries.size();
    qDebug() << "[convertimg] renderPdfWithQt check47 TOC:" << options.generateHyperlinkedTableOfContents;

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

        qDebug() << "[convertimg] TOC rows per page:" << rowsPerTocPage;
        qDebug() << "[convertimg] TOC page count:" << tocPageCount;

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

    qDebug() << "[convertimg] renderPdfWithQt finished. Link annotations:" << linkAnnotations.size();

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
        qDebug() << "[convertimg] addTocLinkAnnotationsWithQpdf input:" << inputPdfPath;
        qDebug() << "[convertimg] addTocLinkAnnotationsWithQpdf output:" << outputPdfPath;
        qDebug() << "[convertimg] addTocLinkAnnotationsWithQpdf annotations:" << linkAnnotations.size();

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

        qDebug() << "[convertimg] qpdf TOC annotation pass finished";

        return true;
    }
    catch (const std::exception& e) {
        error = QObject::tr("Could not add hyperlinked table of contents: %1")
        .arg(QString::fromUtf8(e.what()));
        return false;
    }
}

bool convertImagesToPdf(
    const ConvertImgOptions& options,
    QString& error)
{
    error.clear();

    qDebug() << "[convertimg] convertImagesToPdf started";
    qDebug() << "[convertimg] output:" << options.outputPdfPath;
    qDebug() << "[convertimg] image count:" << options.imagePaths.size();
    qDebug() << "[convertimg] check46 one image per page:" << options.oneImagePerPage;
    qDebug() << "[convertimg] check47 hyperlinked TOC:" << options.generateHyperlinkedTableOfContents;

    if (options.imagePaths.isEmpty()) {
        error = QObject::tr("No images were selected.");
        return false;
    }

    if (options.outputPdfPath.trimmed().isEmpty()) {
        error = QObject::tr("No output PDF path was provided.");
        return false;
    }

    if (!options.oneImagePerPage) {
        error = QObject::tr("The current converter supports only one image per PDF page. Enable the one-image-per-page option.");
        return false;
    }

    const QString finalOutputPath = ensurePdfExtension(options.outputPdfPath);

    if (!options.generateHyperlinkedTableOfContents) {
        QVector<TocLinkAnnotation> unusedLinks;
        const bool ok = renderPdfWithQt(options, finalOutputPath, unusedLinks, error);
        qDebug() << "[convertimg] convertImagesToPdf finished without TOC:" << ok;
        return ok;
    }

    QTemporaryFile temporaryFile(QDir::tempPath() + "/PDFutils_convertimg_XXXXXX.pdf");
    temporaryFile.setAutoRemove(false);

    if (!temporaryFile.open()) {
        error = QObject::tr("Could not create a temporary PDF file.");
        return false;
    }

    const QString temporaryPdfPath = temporaryFile.fileName();
    temporaryFile.close();
    QFile::remove(temporaryPdfPath);

    qDebug() << "[convertimg] temporary PDF for TOC path:" << temporaryPdfPath;

    QVector<TocLinkAnnotation> linkAnnotations;

    if (!renderPdfWithQt(options, temporaryPdfPath, linkAnnotations, error)) {
        QFile::remove(temporaryPdfPath);
        return false;
    }

    if (!addTocLinkAnnotationsWithQpdf(temporaryPdfPath, finalOutputPath, linkAnnotations, error)) {
        QFile::remove(temporaryPdfPath);
        return false;
    }

    QFile::remove(temporaryPdfPath);

    qDebug() << "[convertimg] convertImagesToPdf finished with TOC";

    return true;
}

} // namespace

convertimg::convertimg(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::convertimg)
{
    ui->setupUi(this);

    qDebug() << "[convertimg] constructor reached";

    configureImageTable();
    connectUiSignals();
    resetEmptyImageRows();
}

convertimg::~convertimg()
{
    qDebug() << "[convertimg] destructor reached";
    delete ui;
}

void convertimg::connectUiSignals()
{
    qDebug() << "[convertimg] connectUiSignals reached";
    qDebug() << "[convertimg] btn45a pointer:" << ui->btn45a;
    qDebug() << "[convertimg] btn45b pointer:" << ui->btn45b;
    qDebug() << "[convertimg] btn45c pointer:" << ui->btn45c;
    qDebug() << "[convertimg] btn48 pointer:" << ui->btn48;
    qDebug() << "[convertimg] check46 pointer:" << ui->check46;
    qDebug() << "[convertimg] check47 pointer:" << ui->check47;
    qDebug() << "[convertimg] tbl44 pointer:" << ui->tbl44;

    const QMetaObject::Connection addConnection =
        connect(ui->btn45a, &QPushButton::clicked, this, &convertimg::addImageFiles);
    const QMetaObject::Connection removeConnection =
        connect(ui->btn45b, &QPushButton::clicked, this, &convertimg::removeSelectedImageRows);
    const QMetaObject::Connection clearConnection =
        connect(ui->btn45c, &QPushButton::clicked, this, &convertimg::clearImageTable);
    const QMetaObject::Connection runConnection =
        connect(ui->btn48, &QPushButton::clicked, this, &convertimg::runConversion);

    qDebug() << "[convertimg] btn45a connected:" << static_cast<bool>(addConnection);
    qDebug() << "[convertimg] btn45b connected:" << static_cast<bool>(removeConnection);
    qDebug() << "[convertimg] btn45c connected:" << static_cast<bool>(clearConnection);
    qDebug() << "[convertimg] btn48 connected:" << static_cast<bool>(runConnection);
}

void convertimg::configureImageTable()
{
    ui->tbl44->setColumnCount(3);

    QStringList headers;
    headers << tr("Path") << tr("File size") << tr("Dimensions");
    ui->tbl44->setHorizontalHeaderLabels(headers);

    ui->tbl44->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tbl44->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tbl44->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tbl44->verticalHeader()->setVisible(false);
    ui->tbl44->horizontalHeader()->setStretchLastSection(false);

    qDebug() << "[convertimg] tbl44 configured";
}

void convertimg::resetEmptyImageRows()
{
    if (hasImageRows()) {
        return;
    }

    ui->tbl44->setRowCount(PlaceholderRowCount);
    ui->tbl44->resizeColumnsToContents();

    qDebug() << "[convertimg] placeholder rows reset:" << PlaceholderRowCount;
}

void convertimg::addImageFiles()
{
    qDebug() << "[convertimg] addImageFiles reached";

    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select image files"),
        QDir::homePath(),
        imageFileDialogFilter(),
        nullptr,
        QFileDialog::DontUseNativeDialog
        );

    if (files.isEmpty()) {
        qDebug() << "[convertimg] no image files selected";
        return;
    }

    qDebug() << "[convertimg] selected image files:" << files;

    // Remove empty placeholder rows before appending actual image rows.
    for (int row = ui->tbl44->rowCount() - 1; row >= 0; --row) {
        QTableWidgetItem* item = ui->tbl44->item(row, ImageTableColumns().path);

        if (!item || item->text().trimmed().isEmpty()) {
            ui->tbl44->removeRow(row);
        }
    }

    ImageTableColumns columns;
    columns.path = 0;
    columns.fileSize = 1;
    columns.dimensions = 2;

    QStringList errors;

    appendImageFilesToTableWidget(
        ui->tbl44,
        files,
        errors,
        columns
        );

    if (!errors.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Some images could not be added"),
            errors.join('\n')
            );
    }

    ui->tbl44->resizeColumnsToContents();

    qDebug() << "[convertimg] tbl44 row count after add:" << ui->tbl44->rowCount();
}

void convertimg::removeSelectedImageRows()
{
    qDebug() << "[convertimg] removeSelectedImageRows reached";

    const QList<QTableWidgetItem*> selectedItems = ui->tbl44->selectedItems();

    QSet<int> rowsToRemove;

    for (QTableWidgetItem* item : selectedItems) {
        rowsToRemove.insert(item->row());
    }

    QList<int> rows = rowsToRemove.values();
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    for (int row : rows) {
        ui->tbl44->removeRow(row);
    }

    resetEmptyImageRows();

    qDebug() << "[convertimg] removed row count:" << rows.size();
    qDebug() << "[convertimg] tbl44 row count after remove:" << ui->tbl44->rowCount();
}

void convertimg::clearImageTable()
{
    qDebug() << "[convertimg] clearImageTable reached";

    ui->tbl44->setRowCount(0);
    resetEmptyImageRows();
}

QStringList convertimg::imagePathsFromTable() const
{
    QStringList paths;

    const ImageTableColumns columns;

    for (int row = 0; row < ui->tbl44->rowCount(); ++row) {
        QTableWidgetItem* item = ui->tbl44->item(row, columns.path);

        if (!item) {
            continue;
        }

        const QString path = item->text().trimmed();

        if (!path.isEmpty()) {
            paths.append(path);
        }
    }

    return paths;
}

bool convertimg::hasImageRows() const
{
    return !imagePathsFromTable().isEmpty();
}

ConvertImgOptions convertimg::collectOptions(const QString& outputPdfPath) const
{
    ConvertImgOptions options;

    options.imagePaths = imagePathsFromTable();
    options.outputPdfPath = outputPdfPath.trimmed();
    options.oneImagePerPage = ui->check46->isChecked();
    options.generateHyperlinkedTableOfContents = ui->check47->isChecked();

    if (options.generateHyperlinkedTableOfContents && !options.oneImagePerPage) {
        options.oneImagePerPage = true;
    }

    qDebug() << "[convertimg] collected options output:" << options.outputPdfPath;
    qDebug() << "[convertimg] collected options image count:" << options.imagePaths.size();
    qDebug() << "[convertimg] collected options check46:" << options.oneImagePerPage;
    qDebug() << "[convertimg] collected options check47:" << options.generateHyperlinkedTableOfContents;

    return options;
}

void convertimg::runConversion()
{
    qDebug() << "[convertimg] runConversion reached";
    qDebug() << "[convertimg] check46 checked:" << ui->check46->isChecked();
    qDebug() << "[convertimg] check47 checked:" << ui->check47->isChecked();
    qDebug() << "[convertimg] current image paths:" << imagePathsFromTable();

    if (!hasImageRows()) {
        QMessageBox::warning(
            this,
            tr("No images selected"),
            tr("Add at least one image before converting.")
            );
        return;
    }

    if (ui->check47->isChecked() && !ui->check46->isChecked()) {
        qDebug() << "[convertimg] check47 requires check46; enabling check46";
        ui->check46->setChecked(true);
    }

    if (!ui->check46->isChecked()) {
        QMessageBox::warning(
            this,
            tr("Unsupported layout"),
            tr("The current converter supports only one image per PDF page. Enable the one-image-per-page option.")
            );
        return;
    }

    QString outputPath = QFileDialog::getSaveFileName(
        this,
        tr("Save image PDF"),
        QDir::homePath() + "/images.pdf",
        tr("PDF files (*.pdf)"),
        nullptr,
        QFileDialog::DontUseNativeDialog
        );

    if (outputPath.trimmed().isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Choose a destination"),
            tr("Destination file path cannot be empty.")
            );
        return;
    }

    outputPath = ensurePdfExtension(outputPath);

    qDebug() << "[convertimg] selected output path:" << outputPath;

    if (QFileInfo::exists(outputPath)) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            tr("Overwrite PDF"),
            tr("The file already exists:\n%1\n\nOverwrite it?").arg(outputPath),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
            );

        if (answer != QMessageBox::Yes) {
            qDebug() << "[convertimg] user declined overwrite";
            return;
        }
    }

    const ConvertImgOptions options = collectOptions(outputPath);

    QString error;

    if (!convertImagesToPdf(options, error)) {
        qDebug() << "[convertimg] conversion failed:" << error;
        QMessageBox::critical(
            this,
            tr("Conversion failed"),
            error
            );
        return;
    }

    QString message = tr("The image PDF was created successfully.");

    if (options.generateHyperlinkedTableOfContents) {
        message += "\n\n" + tr("A hyperlinked table of contents was added at the beginning of the PDF.");
    }

    qDebug() << "[convertimg] conversion complete";

    QMessageBox::information(
        this,
        tr("Conversion complete"),
        message
        );
}
