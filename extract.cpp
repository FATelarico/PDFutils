#include "extract.h"
#include "ui_extract.h"

#include "merge_helpers.cpp"
#include "split_helpers.cpp"
#include "extract_helpers.cpp"

#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <functional>

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

        // ui->tbl24->setColumnCount(3); Already done elsewhere

        // Remove empty rows added for aesthetics
        for (int row = ui->tbl24->rowCount() - 1; row >= 0; --row) {
            QTableWidgetItem* item = ui->tbl24->item(row, 0);

            if (!item || item->text().trimmed().isEmpty()) {
                ui->tbl24->removeRow(row);
            }
        }

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
        };

        // Resize the column according to the contents
        ui->tbl24->resizeColumnsToContents();
    });

    connect(ui->btn25b, &QPushButton::clicked, this, [this]() {
        const QList<QTableWidgetItem*> selectedItems =
            ui->tbl24->selectedItems();

        QSet<int> rowsToRemove;

        for (QTableWidgetItem* item : selectedItems) {
            rowsToRemove.insert(item->row());
        }

        QList<int> rows = rowsToRemove.values();
        std::sort(rows.begin(), rows.end(), std::greater<int>());

        for (int row : qAsConst(rows)) {
            ui->tbl24->removeRow(row);
        }

        if (ui->tbl24->rowCount() == 0) {
            ui->tbl24->setRowCount(7);
            ui->tbl24->resizeColumnsToContents();
        }
    });

    connect(ui->btn25c, &QPushButton::clicked, this, [this]() {
        ui->tbl24->setRowCount(0);
        ui->tbl24->setRowCount(7);
        ui->tbl24->resizeColumnsToContents();
    });

    connect(ui->btn28, &QPushButton::clicked, this, [this]() {
        const int pathColumn = 0;

        const QStringList pdfPaths =
            getPdfPathsFromTableWidget(ui->tbl24, pathColumn);

        if (pdfPaths.isEmpty()) {
            QMessageBox::warning(
                this,
                "No PDF files",
                "No PDF files were provided."
                );
            return;
        }

        QString parseError;

        QVector<DocumentPageSelection> selections =
            parsePageSelections(
                ui->lineEdit26->text(),
                pdfPaths.size(),
                parseError
                );

        if (!parseError.isEmpty()) {
            QMessageBox::warning(
                this,
                "Invalid page range",
                parseError
                );
            return;
        }

        QVector<int> pageCounts;
        QString pageCountError;

        if (!getPdfPageCountsWithLibQpdf(
                pdfPaths,
                pageCounts,
                pageCountError)) {
            QMessageBox::warning(
                this,
                "Could not read PDF page counts",
                pageCountError
                );
            return;
        }

        QString validationError;

        if (!validateSelectionsAgainstPageCounts(
                selections,
                pageCounts,
                validationError)) {
            QMessageBox::warning(
                this,
                "Invalid page range",
                validationError
                );
            return;
        }

        ExtractOutputMode extractMode = ExtractOutputMode::OnePdfPerRange;

        if (ui->radio27a->isChecked()) {
            extractMode = ExtractOutputMode::OnePdfPerRange;
        }
        else if (ui->radio27b->isChecked()) {
            extractMode = ExtractOutputMode::OnePdfPerInputFile;
        }
        else if (ui->radio27c->isChecked()) {
            extractMode = ExtractOutputMode::OnePdfForAllInputFiles;
        }
        else {
            QMessageBox::warning(
                this,
                "No extraction mode selected",
                "Choose an extraction output mode."
                );
            return;
        }

        /*
        This is the same pattern as splitting: the selected path supplies
        the output folder and filename suffix. It is not a single final PDF.
        */
        const QString outputPath = QFileDialog::getSaveFileName(
            this,
            "Extract PDF(s) here",
            QDir::homePath() + "/_extract.pdf",
            "PDF files (*.pdf)",
            nullptr,
            QFileDialog::DontUseNativeDialog
            );

        if (outputPath.isEmpty()) {
            QMessageBox::warning(
                this,
                "Choose a destination",
                "Destination file's path cannot be empty."
                );
            return;
        }

        QStringList createdFiles;
        QString extractError;

        if (extractMode == ExtractOutputMode::OnePdfPerRange) {
            QVector<QVector<PageRange>> extractRangesByDocument;
            QString rangeError;

            if (!resolveExtractRangesFromSelections(
                    selections,
                    pageCounts,
                    extractRangesByDocument,
                    rangeError)) {
                QMessageBox::warning(
                    this,
                    "Invalid extraction ranges",
                    rangeError
                    );
                return;
            }

            if (!extractPdfsWithLibQpdf(
                    pdfPaths,
                    extractRangesByDocument,
                    outputPath,
                    createdFiles,
                    extractError)) {
                QMessageBox::critical(
                    this,
                    "Extraction failed",
                    extractError
                    );
                return;
            }
        }
        else if (extractMode == ExtractOutputMode::OnePdfPerInputFile) {
            if (!extractPdfsMergedByFileWithLibQpdf(
                    pdfPaths,
                    selections,
                    outputPath,
                    createdFiles,
                    extractError)) {
                QMessageBox::critical(
                    this,
                    "Extraction failed",
                    extractError
                    );
                return;
            }
        }
        else {
            if (!extractPdfsMergedGloballyWithLibQpdf(
                    pdfPaths,
                    selections,
                    outputPath,
                    createdFiles,
                    extractError)) {
                QMessageBox::critical(
                    this,
                    "Extraction failed",
                    extractError
                    );
                return;
            }
        }

        const QFileInfo outputInfo(outputPath);

        QMessageBox::information(
            this,
            "Extraction complete",
            QString("Created %1 PDF file(s) in:\n%2")
                .arg(createdFiles.size())
                .arg(outputInfo.absoluteDir().absolutePath())
            );
    });
}

extract::~extract()
{
    delete ui;
}
