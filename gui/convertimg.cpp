#include "convertimg.h"
#include "ui_convertimg.h"

#include "image_to_pdf_service.h"

#include <QAbstractItemView>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QImageReader>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <algorithm>
#include <functional>

namespace
{
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
        error = QObject::tr("Unsupported or unreadable image file: %1")
            .arg(fileInfo.absoluteFilePath());
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

    tableWidget->setItem(row, columns.fileSize, sizeItem);

    QTableWidgetItem* dimensionsItem = new QTableWidgetItem(formatDimensions(rowData.dimensions));
    dimensionsItem->setData(Qt::UserRole, rowData.dimensions);
    dimensionsItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    tableWidget->setItem(row, columns.dimensions, dimensionsItem);
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
} // namespace

convertimg::convertimg(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::convertimg)
{
    ui->setupUi(this);

    configureImageTable();
    connectUiSignals();
    resetEmptyImageRows();
}

convertimg::~convertimg()
{
    delete ui;
}

void convertimg::connectUiSignals()
{
    connect(ui->btn45a, &QPushButton::clicked, this, &convertimg::addImageFiles);
    connect(ui->btn45b, &QPushButton::clicked, this, &convertimg::removeSelectedImageRows);
    connect(ui->btn45c, &QPushButton::clicked, this, &convertimg::clearImageTable);
    connect(ui->btn48, &QPushButton::clicked, this, &convertimg::runConversion);
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
}

void convertimg::resetEmptyImageRows()
{
    if (hasImageRows()) {
        return;
    }

    ui->tbl44->setRowCount(PlaceholderRowCount);
    ui->tbl44->resizeColumnsToContents();
}

void convertimg::addImageFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select image files"),
        QDir::homePath(),
        imageFileDialogFilter(),
        nullptr,
        QFileDialog::DontUseNativeDialog
    );

    if (files.isEmpty()) {
        return;
    }

    for (int row = ui->tbl44->rowCount() - 1; row >= 0; --row) {
        QTableWidgetItem* item = ui->tbl44->item(row, ImageTableColumns().path);

        if (!item || item->text().trimmed().isEmpty()) {
            ui->tbl44->removeRow(row);
        }
    }

    QStringList errors;
    appendImageFilesToTableWidget(ui->tbl44, files, errors);

    if (!errors.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Some images could not be added"),
            errors.join('\n')
        );
    }

    ui->tbl44->resizeColumnsToContents();
}

void convertimg::removeSelectedImageRows()
{
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
}

void convertimg::clearImageTable()
{
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

    return options;
}

void convertimg::runConversion()
{
    if (!hasImageRows()) {
        QMessageBox::warning(
            this,
            tr("No images selected"),
            tr("Add at least one image before converting.")
        );
        return;
    }

    if (ui->check47->isChecked() && !ui->check46->isChecked()) {
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

    outputPath = normaliseImagePdfOutputPath(outputPath);

    if (QFileInfo::exists(outputPath)) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            tr("Overwrite PDF"),
            tr("The file already exists:\n%1\n\nOverwrite it?").arg(outputPath),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    const ConvertImgOptions options = collectOptions(outputPath);
    const OperationResult result = convertImagesToPdf(options);

    if (!result.ok) {
        QMessageBox::critical(this, tr("Conversion failed"), result.message);
        return;
    }

    QMessageBox::information(
        this,
        tr("Conversion complete"),
        result.message
    );
}
