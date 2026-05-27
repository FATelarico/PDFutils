#include "extract.h"
#include "ui_extract.h"

#include "extract_service.h"
#include "qpdf_utils.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStringList>
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
    error.clear();

    QFileInfo fileInfo(pdfPath.trimmed());

    if (!fileInfo.exists()) {
        error = QObject::tr("File does not exist: %1").arg(pdfPath);
        return false;
    }

    if (!fileInfo.isFile()) {
        error = QObject::tr("Path is not a file: %1").arg(pdfPath);
        return false;
    }

    int pageCount = 0;
    const OperationResult pageCountResult = getPdfPageCount(
        fileInfo.absoluteFilePath(),
        pageCount
    );

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

void removeEmptyRows(QTableWidget* tableWidget, int pathColumn)
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

void resetPlaceholderRows(QTableWidget* tableWidget)
{
    if (!tableWidget) {
        return;
    }

    if (getPdfPathsFromTableWidget(tableWidget).isEmpty()) {
        tableWidget->setRowCount(7);
        tableWidget->resizeColumnsToContents();
    }
}

ExtractOptions::OutputMode outputModeFromUi(Ui::extract* ui)
{
    if (ui->radio27b->isChecked()) {
        return ExtractOptions::OutputMode::OnePdfPerInputFile;
    }

    if (ui->radio27c->isChecked()) {
        return ExtractOptions::OutputMode::OnePdfForAllInputFiles;
    }

    return ExtractOptions::OutputMode::OnePdfPerRange;
}
} // namespace

extract::extract(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::extract)
{
    ui->setupUi(this);

    connect(ui->btn25a, &QPushButton::clicked, this, [this]() {
        const QStringList files = QFileDialog::getOpenFileNames(
            this,
            tr("Select PDF files"),
            QDir::homePath(),
            tr("Portable Document Files (*.pdf)"),
            nullptr,
            QFileDialog::DontUseNativeDialog
        );

        if (files.isEmpty()) {
            return;
        }

        removeEmptyRows(ui->tbl24, 0);

        PdfTableColumns columns;
        columns.path = 0;
        columns.fileSize = 1;
        columns.pageCount = 2;

        QStringList errors;

        appendPdfFilesToTableWidget(
            ui->tbl24,
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

        ui->tbl24->resizeColumnsToContents();
    });

    connect(ui->btn25b, &QPushButton::clicked, this, [this]() {
        const QList<QTableWidgetItem*> selectedItems = ui->tbl24->selectedItems();

        QSet<int> rowsToRemove;

        for (QTableWidgetItem* item : selectedItems) {
            rowsToRemove.insert(item->row());
        }

        QList<int> rows = rowsToRemove.values();
        std::sort(rows.begin(), rows.end(), std::greater<int>());

        for (int row : rows) {
            ui->tbl24->removeRow(row);
        }

        resetPlaceholderRows(ui->tbl24);
    });

    connect(ui->btn25c, &QPushButton::clicked, this, [this]() {
        ui->tbl24->setRowCount(0);
        resetPlaceholderRows(ui->tbl24);
    });

    connect(ui->btn28, &QPushButton::clicked, this, [this]() {
        const QStringList pdfPaths = getPdfPathsFromTableWidget(ui->tbl24, 0);

        if (pdfPaths.isEmpty()) {
            QMessageBox::warning(
                this,
                tr("No PDF files"),
                tr("No PDF files were provided.")
            );
            return;
        }

        const QString outputPath = QFileDialog::getSaveFileName(
            this,
            tr("Extract PDF(s) here"),
            QDir::homePath() + "/_extract.pdf",
            tr("PDF files (*.pdf)"),
            nullptr,
            QFileDialog::DontUseNativeDialog
        );

        if (outputPath.trimmed().isEmpty()) {
            QMessageBox::warning(
                this,
                tr("Choose a destination"),
                tr("Destination file's path cannot be empty.")
            );
            return;
        }

        ExtractOptions options;
        options.inputPaths = pdfPaths;
        options.outputPath = outputPath;
        options.pageRangeSpec = ui->lineEdit26->text();
        options.outputMode = outputModeFromUi(ui);

        QStringList createdFiles;
        const OperationResult result = extractPdfs(options, &createdFiles);

        if (!result.ok) {
            QMessageBox::critical(
                this,
                tr("Extraction failed"),
                result.message
            );
            return;
        }

        const QFileInfo outputInfo(outputPath);

        QMessageBox::information(
            this,
            tr("Extraction complete"),
            tr("Created %1 PDF file(s) in:\n%2")
                .arg(createdFiles.size())
                .arg(outputInfo.absoluteDir().absolutePath())
        );
    });
}

extract::~extract()
{
    delete ui;
}
