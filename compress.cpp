#include "merge_helpers.cpp"
#include "split_helpers.cpp"
#include "compress_helpers.cpp"
#include "helptexts.cpp"

#include "compress.h"
#include "ui_compress.h"

#include <QSettings>
#include <QFileInfo>
#include <QFileDialog>
#include <QWidget>
#include <QMessageBox>
#include <QDir>
#include <QSet>
#include <algorithm>
#include <functional>

compress::compress(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::compress)
{
    ui->setupUi(this);

    connect(ui->tool39, &QToolButton::clicked, this, [this]() {
        QMessageBox msgBox(this);

        msgBox.setWindowTitle(tr("Page range help"));
        msgBox.setIcon(QMessageBox::Information);

        msgBox.setText(tr("<b>Page range options</b>"));

        msgBox.setInformativeText(pageCompressHelpText1());

        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    });

    connect(ui->tool42c, &QToolButton::clicked, this, [this]() {
        QMessageBox msgBox(this);

        msgBox.setWindowTitle(tr("Page range help"));
        msgBox.setIcon(QMessageBox::Information);

        msgBox.setText(tr("<b>Page range options</b>"));

        msgBox.setInformativeText(pageCompressHelpText2());

        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    });

    connect(ui->tool43, &QToolButton::clicked, this, [this]() {
        QMessageBox msgBox(this);

        msgBox.setWindowTitle(tr("Page range help"));
        msgBox.setIcon(QMessageBox::Information);

        msgBox.setText(tr("<b>Page range options</b>"));

        msgBox.setInformativeText(pageCompressHelpText2());

        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    });

    connect(ui->btn38a, &QPushButton::clicked, this, [this]() {
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

        // ui->tbl37->setColumnCount(3); Already done elsewhere.

        for (int row = ui->tbl37->rowCount() - 1; row >= 0; --row) {
            QTableWidgetItem* item = ui->tbl37->item(row, 0);

            if (!item || item->text().trimmed().isEmpty()) {
                ui->tbl37->removeRow(row);
            }
        }

        PdfTableColumns columns;
        columns.path = 0;
        columns.fileSize = 1;
        columns.pageCount = 2;

        QStringList errors;

        appendPdfFilesToTableWidget(
            ui->tbl37,
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

        ui->tbl37->resizeColumnsToContents();
    });

    connect(ui->btn38b, &QPushButton::clicked, this, [this]() {
        const QList<QTableWidgetItem*> selectedItems = ui->tbl37->selectedItems();
        QSet<int> rowsToRemove;

        for (QTableWidgetItem* item : selectedItems) {
            rowsToRemove.insert(item->row());
        }

        QList<int> rows = rowsToRemove.values();
        std::sort(rows.begin(), rows.end(), std::greater<int>());

        for (int row : qAsConst(rows)) {
            ui->tbl37->removeRow(row);
        }

        if (ui->tbl37->rowCount() == 0) {
            ui->tbl37->setRowCount(5);
            ui->tbl37->resizeColumnsToContents();
        }
    });

    connect(ui->btn38c, &QPushButton::clicked, this, [this]() {
        ui->tbl37->setRowCount(0);
        ui->tbl37->setRowCount(5);
        ui->tbl37->resizeColumnsToContents();
    });

    connect(ui->tool44, &QToolButton::clicked, this, [this]() {
#ifdef Q_OS_WIN
        const QString filter =
            tr("Ghostscript console executable (gswin64c.exe gswin32c.exe);;Executables (*.exe);;All files (*)");
#else
        const QString filter =
            tr("Ghostscript executable (gs);;All files (*)");
#endif

        QString startDir = QDir::homePath();

        if (!m_ghostscriptProgram.trimmed().isEmpty()) {
            const QFileInfo currentInfo(m_ghostscriptProgram);

            if (currentInfo.exists()) {
                startDir = currentInfo.absolutePath();
            }
        }

        const QString program = QFileDialog::getOpenFileName(
            this,
            tr("Select Ghostscript executable"),
            startDir,
            filter,
            nullptr,
            QFileDialog::DontUseNativeDialog
            );

        if (program.isEmpty()) {
            return;
        }

        const QFileInfo programInfo(program);

        if (!programInfo.exists() || !programInfo.isFile()) {
            QMessageBox::warning(
                this,
                tr("Invalid Ghostscript executable"),
                tr("The selected path is not a valid file.")
                );
            return;
        }

#ifndef Q_OS_WIN
        if (!programInfo.isExecutable()) {
            QMessageBox::warning(
                this,
                tr("Invalid Ghostscript executable"),
                tr("The selected file is not executable.")
                );
            return;
        }
#endif

#ifdef Q_OS_WIN
        const QString fileName = programInfo.fileName().toLower();

        if (fileName != QStringLiteral("gswin64c.exe") &&
            fileName != QStringLiteral("gswin32c.exe") &&
            fileName != QStringLiteral("gs.exe")) {
            const QMessageBox::StandardButton answer = QMessageBox::question(
                this,
                tr("Unexpected executable"),
                tr("The selected file does not look like the Ghostscript console executable.\n\n"
                   "Expected gswin64c.exe or gswin32c.exe.\n\n"
                   "Use it anyway?")
                );

            if (answer != QMessageBox::Yes) {
                return;
            }
        }
#endif

        m_ghostscriptProgram = programInfo.absoluteFilePath();

        QSettings settings;
        settings.setValue(
            QStringLiteral("compression/ghostscriptProgram"),
            m_ghostscriptProgram
            );

        ui->tool44->setToolTip(
            tr("Ghostscript executable:\n%1").arg(m_ghostscriptProgram)
            );
    });

    connect(ui->btn44, &QPushButton::clicked, this, [this]() {
        if (!ui->radio39a->isChecked() && !ui->radio39b->isChecked()) {
            QMessageBox::warning(
                this,
                tr("No compression backend selected"),
                tr("Select either qpdf or Ghostscript before starting compression.")
                );
            return;
        }

        const bool useQpdf = ui->radio39a->isChecked();
        const bool useGhostscript = ui->radio39b->isChecked();

        if (useQpdf && !ui->radio41a->isChecked() && !ui->radio41b->isChecked()) {
            QMessageBox::warning(
                this,
                tr("No qpdf compression mode selected"),
                tr("Select a qpdf compression mode before starting compression.")
                );
            return;
        }

        if (useGhostscript && !ui->radio42a->isChecked() && !ui->radio42c->isChecked()) {
            QMessageBox::warning(
                this,
                tr("No Ghostscript compression mode selected"),
                tr("Select either the Ghostscript preset mode or custom downsampling mode before starting compression.")
                );
            return;
        }

        const int pathColumn = 0;
        const QStringList inputPdfPaths = getPdfPathsFromTableWidget(ui->tbl37, pathColumn);

        if (inputPdfPaths.isEmpty()) {
            QMessageBox::warning(
                this,
                tr("No PDF files"),
                tr("No PDF files were provided.")
                );
            return;
        }

        const QString outputPath = QFileDialog::getSaveFileName(
            this,
            tr("Save compressed PDF output"),
            QDir::homePath() + "/compressed.pdf",
            tr("PDF files (*.pdf)"),
            nullptr,
            QFileDialog::DontUseNativeDialog
            );

        if (outputPath.isEmpty()) {
            QMessageBox::warning(
                this,
                tr("Choose a destination"),
                tr("Destination file's path cannot be empty.")
                );
            return;
        }

        PdfCompressionOptions options;
        options.backend = useGhostscript
                              ? PdfCompressionBackend::Ghostscript
                              : PdfCompressionBackend::Qpdf;
        options.removeMetadata = ui->check41->isChecked();

        if (useGhostscript) {
            options.ghostscriptProgram = m_ghostscriptProgram;
        }

        if (useQpdf) {
            options.mode = ui->radio41b->isChecked()
            ? PdfCompressionMode::StructureAndImages
            : PdfCompressionMode::StructureOnly;
        }

        if (useGhostscript && ui->radio42a->isChecked()) {
            options.ghostscriptMode = GhostscriptCompressionMode::Preset;
            options.ghostscriptPdfSettings = ghostscriptPdfSettingsFromComboIndex(
                ui->combo42a->currentIndex()
                );
            options.ghostscriptCompatibilityLevel = ghostscriptCompatibilityLevelFromComboIndex(
                ui->combo42b->currentIndex()
                );
        }

        if (useGhostscript && ui->radio42c->isChecked()) {
            options.ghostscriptMode = GhostscriptCompressionMode::CustomDownsampling;
            options.downsampleColorImages = ui->check43a->isChecked();
            options.downsampleGrayImages = ui->check43b->isChecked();
            options.downsampleMonoImages = ui->check43c->isChecked();
            options.colorImageResolution = ui->spin43a->value();
            options.grayImageResolution = ui->spin43b->value();
            options.monoImageResolution = ui->spin43c->value();
            options.colorImageDownsampleThreshold = ui->dspin43a->value();
            options.grayImageDownsampleThreshold = ui->dspin43b->value();
            options.monoImageDownsampleThreshold = ui->dspin43c->value();
        }

        QVector<PdfCompressionResult> results;
        QString compressionError;

        if (!compressPdfs(
                inputPdfPaths,
                outputPath,
                options,
                results,
                compressionError)) {
            QMessageBox::critical(
                this,
                tr("Compression failed"),
                compressionError
                );
            return;
        }

        QMessageBox::information(
            this,
            tr("Compression complete"),
            makePdfCompressionReport(results)
            );
    });
};

compress::~compress()
{
    delete ui;
}
