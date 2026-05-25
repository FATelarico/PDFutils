#include "merge.h"
#include "ui_merge.h"
#include "merge_helpers.cpp"
#include "helptexts.cpp"

#include <QFileDialog>
#include <QDir>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QStringList>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <functional>
#include <QMessageBox>

merge::merge(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::merge)
{
    ui->setupUi(this);

    connect(ui->tool11, &QToolButton::clicked, this, [this]() {
        QMessageBox msgBox(this);

        msgBox.setWindowTitle(tr("Page range help"));
        msgBox.setIcon(QMessageBox::Information);

        msgBox.setText(tr("<b>Page range options</b>"));

        msgBox.setInformativeText(pageRangeHelpText());

        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    });

    connect(ui->btn10a, &QPushButton::clicked, this, [this]() {
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

        // ui->tbl09->setColumnCount(3); Already done elsewhere

        // Remove empty rows added for aesthetics
        for (int row = ui->tbl09->rowCount() - 1; row >= 0; --row) {
            QTableWidgetItem* item = ui->tbl09->item(row, 0);

            if (!item || item->text().trimmed().isEmpty()) {
                ui->tbl09->removeRow(row);
            }
        }

        PdfTableColumns columns;
        columns.path = 0;
        columns.fileSize = 1;
        columns.pageCount = 2;

        QStringList errors;

        appendPdfFilesToTableWidget(
            ui->tbl09,
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
        ui->tbl09->resizeColumnsToContents();
    });

    connect(ui->btn10b, &QPushButton::clicked, this, [this]() {
        const QList<QTableWidgetItem *> selectedItems =
            ui->tbl09->selectedItems();

        QSet<int> rowsToRemove;

        for (QTableWidgetItem *item : selectedItems) {
            rowsToRemove.insert(item->row());
        }

        QList<int> rows = rowsToRemove.values();
        std::sort(rows.begin(), rows.end(), std::greater<int>());

        for (int row : qAsConst(rows)) {
            ui->tbl09->removeRow(row);
        }

        // Add empty rows for aestethic if no file left
        if (ui->tbl09->rowCount() == 0) {
            ui->tbl09->setRowCount(7);
            ui->tbl09->resizeColumnsToContents();
        }
    });

    connect(ui->btn10c, &QPushButton::clicked, this, [this]() {
        ui->tbl09->setRowCount(0);
        ui->tbl09->setRowCount(7);
        ui->tbl09->resizeColumnsToContents();
    });

    connect(ui->btn15, &QPushButton::clicked, this, [this]() {
        // Parse page ranges
        const int documentCount = ui->tbl09->rowCount();

        QString error;
        QVector<DocumentPageSelection> selections =
            parsePageSelections(ui->lineEdit11->text(), documentCount, error);

        if (!error.isEmpty()) {
            QMessageBox::warning(this, "Invalid page range", error);
            return;
        } /* else {
             debugPrintSelections(selections); // For checking that the parser works
        } */

        // Validate the requested pages
        const int pathColumn = 0; // Column where the file path is stored

        const QStringList pdfPaths =
            getPdfPathsFromTableWidget(ui->tbl09, pathColumn);

        QVector<int> pageCounts;
        QString pageCountError;

        if (!getPdfPageCountsWithLibQpdf(pdfPaths, pageCounts, pageCountError)) {
            QMessageBox::warning(this, "Could not read PDF page counts", pageCountError);
            return;
        }

        /*
        qDebug() << "PDF paths:" << pdfPaths;
        qDebug() << "Page counts:" << pageCounts;

        QMessageBox::information(this, "Validation successful", "The requested page ranges are valid.");
        */

        QString validationError;

        if (!validateSelectionsAgainstPageCounts(selections, pageCounts, validationError)) {
            QMessageBox::warning(this, "Invalid page range", validationError);
            return;
        }

        const QString outputPath = QFileDialog::getSaveFileName(
            this,
            "Save merged PDF",
            QDir::homePath() + "/merged.pdf",
            "PDF files (*.pdf)",
            nullptr,
            QFileDialog::DontUseNativeDialog
        );


        if (outputPath.isEmpty()){
            QMessageBox::warning(this, "Choose a destination", "Destination file's path cannot be empty");
            return;
        }

        QString mergeError;

        if (!mergeSelectedPagesWithLibQpdf(pdfPaths, selections, outputPath, mergeError)) {
            QMessageBox::critical(this, "Merge failed", mergeError);
            return;
        }

        QMessageBox::information(this, "Merge complete", "The PDF files were merged successfully.");
    });
}

merge::~merge()
{
    delete ui;
}
