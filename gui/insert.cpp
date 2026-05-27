#include "insert.h"
#include "ui_insert.h"

#include "insert_service.h"
#include "qpdf_utils.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <algorithm>
#include <functional>

namespace
{

struct PdfTableColumns
{
    int path = 0;
    int fileSize = 1;
    int pageCount = 2;
};

struct PdfFileTableRowData
{
    QString path;
    qint64 fileSizeBytes = 0;
    int pageCount = 0;
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

QStringList getPdfPathsFromTableWidget(
    QTableWidget* tableWidget,
    int pathColumn = 0)
{
    QStringList paths;

    if (!tableWidget) {
        return paths;
    }

    if (pathColumn < 0 || pathColumn >= tableWidget->columnCount()) {
        return paths;
    }

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem* item = tableWidget->item(row, pathColumn);

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

bool getPdfFileTableRowData(
    const QString& pdfPath,
    PdfFileTableRowData& rowData,
    QString& error)
{
    QFileInfo fileInfo(pdfPath);

    if (!fileInfo.exists()) {
        error = QObject::tr("File does not exist: %1").arg(pdfPath);
        return false;
    }

    if (!fileInfo.isFile()) {
        error = QObject::tr("Path is not a file: %1").arg(pdfPath);
        return false;
    }

    int pageCount = 0;
    const OperationResult pageCountResult = getPdfPageCount(pdfPath, pageCount);

    if (!pageCountResult.ok) {
        error = QObject::tr("Could not read page count for %1: %2")
            .arg(pdfPath, pageCountResult.message);
        return false;
    }

    rowData.path = fileInfo.absoluteFilePath();
    rowData.fileSizeBytes = fileInfo.size();
    rowData.pageCount = pageCount;

    return true;
}

void setPdfFileTableRow(
    QTableWidget* tableWidget,
    int row,
    const PdfFileTableRowData& rowData,
    const PdfTableColumns& columns = PdfTableColumns())
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

    QTableWidgetItem* pageCountItem = new QTableWidgetItem(QString::number(rowData.pageCount));
    pageCountItem->setData(Qt::UserRole, rowData.pageCount);
    pageCountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    tableWidget->setItem(
        row,
        columns.pageCount,
        pageCountItem
    );
}

bool appendPdfFileToTableWidget(
    QTableWidget* tableWidget,
    const QString& pdfPath,
    QString& error,
    const PdfTableColumns& columns = PdfTableColumns())
{
    if (!tableWidget) {
        error = QObject::tr("Internal error: table widget is null.");
        return false;
    }

    PdfFileTableRowData rowData;

    if (!getPdfFileTableRowData(pdfPath, rowData, error)) {
        return false;
    }

    const int row = tableWidget->rowCount();
    tableWidget->insertRow(row);

    setPdfFileTableRow(tableWidget, row, rowData, columns);

    return true;
}

bool appendPdfFilesToTableWidget(
    QTableWidget* tableWidget,
    const QStringList& pdfPaths,
    QStringList& errors,
    const PdfTableColumns& columns = PdfTableColumns())
{
    bool allOk = true;

    for (const QString& pdfPath : pdfPaths) {
        QString error;

        if (!appendPdfFileToTableWidget(tableWidget, pdfPath, error, columns)) {
            allOk = false;
            errors.append(error);
        }
    }

    return allOk;
}

void removeEmptyRows(QTableWidget* tableWidget, int pathColumn = 0)
{
    if (!tableWidget) {
        return;
    }

    for (int row = tableWidget->rowCount() - 1; row >= 0; --row) {
        QTableWidgetItem* item = tableWidget->item(row, pathColumn);

        if (!item || item->text().trimmed().isEmpty()) {
            tableWidget->removeRow(row);
        }
    }
}

void resetEmptyRows(QTableWidget* tableWidget, int rowCount)
{
    if (!tableWidget) {
        return;
    }

    if (tableWidget->rowCount() == 0) {
        tableWidget->setRowCount(rowCount);
        tableWidget->resizeColumnsToContents();
    }
}

} // namespace

insert::insert(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::insert)
{
    ui->setupUi(this);

    connect(ui->btn30a, &QPushButton::clicked, this, [this]() {
        const QStringList files = QFileDialog::getOpenFileNames(
            this,
            tr("Select PDF files"),
            QDir::homePath(),
            tr("PDF files (*.pdf)"),
            nullptr,
            QFileDialog::DontUseNativeDialog
        );

        if (files.isEmpty()) {
            return;
        }

        removeEmptyRows(ui->tbl29, 0);

        QStringList errors;

        PdfTableColumns columns;
        columns.path = 0;
        columns.fileSize = 1;
        columns.pageCount = 2;

        appendPdfFilesToTableWidget(
            ui->tbl29,
            files,
            errors,
            columns
        );

        if (!errors.isEmpty()) {
            QMessageBox::warning(
                this,
                tr("Some PDFs could not be added"),
                errors.join('\n')
            );
        }

        ui->tbl29->resizeColumnsToContents();
    });

    connect(ui->btn30b, &QPushButton::clicked, this, [this]() {
        const QList<QTableWidgetItem*> selectedItems = ui->tbl29->selectedItems();
        QSet<int> rowsToRemove;

        for (QTableWidgetItem* item : selectedItems) {
            rowsToRemove.insert(item->row());
        }

        QList<int> rows = rowsToRemove.values();
        std::sort(rows.begin(), rows.end(), std::greater<int>());

        for (int row : rows) {
            ui->tbl29->removeRow(row);
        }

        resetEmptyRows(ui->tbl29, 5);
    });

    connect(ui->btn30c, &QPushButton::clicked, this, [this]() {
        ui->tbl29->setRowCount(0);
        ui->tbl29->setRowCount(5);
        ui->tbl29->resizeColumnsToContents();
    });

    connect(ui->btn36, &QPushButton::clicked, this, [this]() {
        InsertOptions options;

        options.inputPaths = getPdfPathsFromTableWidget(ui->tbl29, 0);

        if (options.inputPaths.isEmpty()) {
            QMessageBox::warning(
                this,
                tr("No PDF files"),
                tr("No PDF files were provided.")
            );
            return;
        }

        if (ui->radio34->isChecked()) {
            options.mode = InsertOptions::Mode::Bookmarks;
            options.bookmarkLevel = ui->spin34->value();
        }
        else if (ui->radio33->isChecked()) {
            options.mode = InsertOptions::Mode::EveryNPages;
            options.pagesPerInsertion = ui->spin33->value();
        }
        else if (ui->radio32->isChecked()) {
            options.mode = InsertOptions::Mode::ManualPages;
            options.manualInsertionPages = ui->lineEdit32->text();
        }
        else if (ui->radio31->isChecked()) {
            options.mode = InsertOptions::Mode::Fixed;
            options.fixedModeIndex = ui->combo31->currentIndex();
        }
        else {
            QMessageBox::warning(
                this,
                tr("No insertion mode selected"),
                tr("Choose an insertion mode.")
            );
            return;
        }

        options.insertedPdfPath = QFileDialog::getOpenFileName(
            this,
            tr("Select PDF pages to insert"),
            QDir::homePath(),
            tr("PDF files (*.pdf)"),
            nullptr,
            QFileDialog::DontUseNativeDialog
        );

        if (options.insertedPdfPath.trimmed().isEmpty()) {
            return;
        }

        options.outputPath = QFileDialog::getSaveFileName(
            this,
            tr("Save inserted PDF output"),
            QDir::homePath() + "/_inserted.pdf",
            tr("PDF files (*.pdf)"),
            nullptr,
            QFileDialog::DontUseNativeDialog
        );

        if (options.outputPath.trimmed().isEmpty()) {
            QMessageBox::warning(
                this,
                tr("Choose a destination"),
                tr("Destination file's path cannot be empty.")
            );
            return;
        }

        QStringList createdFiles;
        const OperationResult result = insertPagesIntoPdfs(options, &createdFiles);

        if (!result.ok) {
            QMessageBox::critical(
                this,
                tr("Insertion failed"),
                result.message
            );
            return;
        }

        const QFileInfo outputInfo(options.outputPath);

        QMessageBox::information(
            this,
            tr("Insertion complete"),
            tr("Created %1 PDF file(s) in:\n%2")
                .arg(createdFiles.size())
                .arg(outputInfo.absoluteDir().absolutePath())
        );
    });
}

insert::~insert()
{
    delete ui;
}
