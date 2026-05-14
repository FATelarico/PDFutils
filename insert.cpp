#include "insert.h"
#include "ui_insert.h"

#include "merge_helpers.cpp"
#include "split_helpers.cpp"
#include "insert_helpers.cpp"

#include <QFileDialog>
#include <QWidget>
#include <QMessageBox>

insert::insert(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::insert)
{
    ui->setupUi(this);

    connect(ui->btn30a, &QPushButton::clicked, this, [this]() {
        const QStringList files = QFileDialog::getOpenFileNames(
            this,
            tr("Select PDF files"),
            QString(),
            tr("PDF files (*.pdf)")
            );

        if (files.isEmpty()) {
            return;
        }

        QStringList errors;

        PdfTableColumns columns;
        columns.path = 0;
        columns.fileSize = 1;
        columns.pageCount = 2;

        // Remove empty rows added for aesthetics
        for (int row = ui->tbl29->rowCount() - 1; row >= 0; --row) {
            QTableWidgetItem *item = ui->tbl29->item(row, 0);

            if (!item || item->text().trimmed().isEmpty()) {
                ui->tbl29->removeRow(row);
            }
        }

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
        };

        // Resize the column according to the contents
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

        for (int row : qAsConst(rows)) {
            ui->tbl29->removeRow(row);
        }

        if (ui->tbl29->rowCount() == 0) {
            ui->tbl29->setRowCount(5);
            ui->tbl29->resizeColumnsToContents();
        }
    });

    connect(ui->btn30c, &QPushButton::clicked, this, [this]() {
        ui->tbl29->setRowCount(0);
        ui->tbl29->setRowCount(5);
        ui->tbl29->resizeColumnsToContents();
    });

    connect(ui->btn36, &QPushButton::clicked, this, [this]() {
        const int pathColumn = 0;

        const QStringList inputPdfPaths =
            getPdfPathsFromTableWidget(ui->tbl29, pathColumn);

        if (inputPdfPaths.isEmpty()) {
            QMessageBox::warning(
                this,
                "No PDF files",
                "No PDF files were provided."
                );
            return;
        }

        QVector<int> pageCounts;
        QString pageCountError;

        if (!getPdfPageCountsWithLibQpdf(
                inputPdfPaths,
                pageCounts,
                pageCountError)) {
            QMessageBox::warning(
                this,
                "Could not read PDF page counts",
                pageCountError
                );
            return;
        }

        QVector<QVector<int>> insertionPositionsByDocument;
        QString positionError;

        if (!determineInsertionPositionsFromPreferences(
                ui->radio31->isChecked(),
                ui->combo31->currentIndex(),
                ui->radio32->isChecked(),
                ui->lineEdit32->text(),
                ui->radio33->isChecked(),
                ui->spin33->value(),
                ui->radio34->isChecked(),
                ui->spin34->value(),
                inputPdfPaths,
                pageCounts,
                insertionPositionsByDocument,
                positionError)) {
            QMessageBox::warning(
                this,
                "Invalid insertion options",
                positionError
                );
            return;
        }

        QString validationError;

        if (!validateInsertionPositionsAgainstPageCounts(
                insertionPositionsByDocument,
                pageCounts,
                validationError)) {
            QMessageBox::warning(
                this,
                "Invalid insertion positions",
                validationError
                );
            return;
        }

        const QString insertedPdfPath = QFileDialog::getOpenFileName(
            this,
            "Select PDF pages to insert",
            QDir::homePath(),
            "PDF files (*.pdf)",
            nullptr,
            QFileDialog::DontUseNativeDialog
            );

        if (insertedPdfPath.isEmpty()) {
            return;
        }

        int insertedPageCount = 0;
        QString insertedPageCountError;

        if (!getPdfPageCountWithLibQpdf(
                insertedPdfPath,
                insertedPageCount,
                insertedPageCountError)) {
            QMessageBox::warning(
                this,
                "Could not read inserted PDF",
                insertedPageCountError
                );
            return;
        }

        const QString outputPath = QFileDialog::getSaveFileName(
            this,
            "Save inserted PDF output",
            QDir::homePath() + "/_inserted.pdf",
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
        QString insertError;

        if (!insertPagesIntoPdfsWithLibQpdf(
                inputPdfPaths,
                insertedPdfPath,
                insertionPositionsByDocument,
                outputPath,
                createdFiles,
                insertError)) {
            QMessageBox::critical(
                this,
                "Insertion failed",
                insertError
                );
            return;
        }

        const QFileInfo outputInfo(outputPath);

        QMessageBox::information(
            this,
            "Insertion complete",
            QString("Created %1 PDF file(s) in:\n%2")
                .arg(createdFiles.size())
                .arg(outputInfo.absoluteDir().absolutePath())
            );
    });
}

insert::~insert()
{
    delete ui;
}
