#include "split.h"
#include "ui_split.h"

#include "merge_helpers.cpp"
#include "split_helpers.cpp"
#include "helptexts.cpp"

#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QToolTip>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <functional>

split::split(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::split)
{
    ui->setupUi(this);

    connect(ui->btn17a, &QPushButton::clicked, this, [this]() {
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

        // ui->tbl16->setColumnCount(3); Already done elsewhere

        // Remove empty rows added for aesthetics
        for (int row = ui->tbl16->rowCount() - 1; row >= 0; --row) {
            QTableWidgetItem* item = ui->tbl16->item(row, 0);

            if (!item || item->text().trimmed().isEmpty()) {
                ui->tbl16->removeRow(row);
            }
        }

        PdfTableColumns columns;
        columns.path = 0;
        columns.fileSize = 1;
        columns.pageCount = 2;

        QStringList errors;

        appendPdfFilesToTableWidget(
            ui->tbl16,
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
        ui->tbl16->resizeColumnsToContents();
    });

    connect(ui->btn17b, &QPushButton::clicked, this, [this]() {
        const QList<QTableWidgetItem *> selectedItems =
            ui->tbl16->selectedItems();

        QSet<int> rowsToRemove;

        for (QTableWidgetItem *item : selectedItems) {
            rowsToRemove.insert(item->row());
        }

        QList<int> rows = rowsToRemove.values();
        std::sort(rows.begin(), rows.end(), std::greater<int>());

        for (int row : qAsConst(rows)) {
            ui->tbl16->removeRow(row);
        }

        // Add empty rows for aestethic if no file left
        if (ui->tbl16->rowCount() == 0) {
            ui->tbl16->setRowCount(6);
            ui->tbl16->resizeColumnsToContents();
        }
    });

    connect(ui->btn17c, &QPushButton::clicked, this, [this]() {
        ui->tbl16->setRowCount(0);
        ui->tbl16->setRowCount(6);
        ui->tbl16->resizeColumnsToContents();
    });

    connect(ui->tool18, &QToolButton::clicked, this, [this]() {
        const QString text =
            // tr("<b><i>Split after</i> options</b><br><br>") +
            pageSplitHelpText1();

        const QPoint pos = ui->tool18->mapToGlobal(
            QPoint(ui->tool18->width() / 2, ui->tool18->height())
            );

        QToolTip::showText(pos, text, ui->tool18);
    });

    connect(ui->tool20, &QToolButton::clicked, this, [this]() {
        const QString text =
            // tr("<b><i>Split after</i> options</b><br><br>") +
            pageSplitHelpText2();

        const QPoint pos = ui->tool20->mapToGlobal(
            QPoint(ui->tool20->width() / 2, ui->tool20->height())
            );

        QToolTip::showText(pos, text, ui->tool20);
    });

    connect(ui->tool21, &QToolButton::clicked, this, [this]() {
        const QString text =
            // tr("<b><i>Split after</i> options</b><br><br>") +
            pageSplitHelpText3();

        const QPoint pos = ui->tool21->mapToGlobal(
            QPoint(ui->tool21->width() / 2, ui->tool21->height())
            );

        QToolTip::showText(pos, text, ui->tool21);
    });

    connect(ui->tool22, &QToolButton::clicked, this, [this]() {
        const QString text =
            // tr("<b><i>Split after</i> options</b><br><br>") +
            pageSplitHelpText4();
        const QPoint pos = ui->tool22->mapToGlobal(
            QPoint(ui->tool22->width() / 2, ui->tool22->height())
            );

        QToolTip::showText(pos, text, ui->tool22);
    });
    connect(ui->btn23, &QPushButton::clicked, this, [this]() {
        const int pathColumn = 0;

        const QStringList pdfPaths =
            getPdfPathsFromTableWidget(ui->tbl16, pathColumn);

        if (pdfPaths.isEmpty()) {
            QMessageBox::warning(
                this,
                "No PDF files",
                "No PDF files were provided."
                );
            return;
        }

        QVector<int> pageCounts;
        QString pageCountError;

        if (!getPdfPageCountsWithLibQpdf(pdfPaths, pageCounts, pageCountError)) {
            QMessageBox::warning(
                this,
                "Could not read PDF page counts",
                pageCountError
                );
            return;
        }

        QVector<QVector<PageRange>> splitRangesByDocument;
        QString rangeError;

        const bool useFixedSplitMode = ui->radio18->isChecked();
        const int splitMode = ui->combo18->currentIndex();

        if (!determineSplitRangesFromPreferences(
                ui->radio18->isChecked(),
                ui->combo18->currentIndex(),
                ui->radio20->isChecked(),
                ui->lineEdit20->text(),
                ui->radio21->isChecked(),
                ui->spin21->value(),
                ui->radio22->isChecked(),
                ui->spin22->value(),
                pdfPaths,
                pageCounts,
                splitRangesByDocument,
                rangeError)) {
            QMessageBox::warning(
                this,
                "Invalid split options",
                rangeError
                );
            return;
        }

        QString validationError;

        if (!validateSplitRangesAgainstPageCounts(
                splitRangesByDocument,
                pageCounts,
                validationError)) {
            QMessageBox::warning(
                this,
                "Invalid split ranges",
                validationError
                );
            return;
        }

        const QString outputPath = QFileDialog::getSaveFileName(
            this,
            "Split PDF(s) here",
            QDir::homePath() + "/_split.pdf",
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
        QString splitError;

        if (!splitPdfsWithLibQpdf(
                pdfPaths,
                splitRangesByDocument,
                outputPath,
                createdFiles,
                splitError)) {
            QMessageBox::critical(
                this,
                "Split failed",
                splitError
                );
            return;
        }

        const QFileInfo outputInfo(outputPath);

        QMessageBox::information(
            this,
            "Split complete",
            QString("Created %1 PDF file(s) in:\n%2")
                .arg(createdFiles.size())
                .arg(outputInfo.absoluteDir().absolutePath())
            );
    });
}

split::~split()
{
    delete ui;
}
