#include "compress.h"
#include "ui_compress.h"

#include "compress_service.h"
#include "helptexts.h"
#include "qpdf_utils.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>

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

bool validateGhostscriptExecutable(
    QWidget* parent,
    const QString& program)
{
    const QFileInfo programInfo(program);

    if (!programInfo.exists() || !programInfo.isFile()) {
        QMessageBox::warning(
            parent,
            QObject::tr("Invalid Ghostscript executable"),
            QObject::tr("The selected path is not a valid file.")
        );
        return false;
    }

#ifndef Q_OS_WIN
    if (!programInfo.isExecutable()) {
        QMessageBox::warning(
            parent,
            QObject::tr("Invalid Ghostscript executable"),
            QObject::tr("The selected file is not executable.")
        );
        return false;
    }
#endif

#ifdef Q_OS_WIN
    const QString fileName = programInfo.fileName().toLower();

    if (fileName != QStringLiteral("gswin64c.exe") &&
        fileName != QStringLiteral("gswin32c.exe") &&
        fileName != QStringLiteral("gs.exe")) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            parent,
            QObject::tr("Unexpected executable"),
            QObject::tr("The selected file does not look like the Ghostscript console executable.\n\n"
                        "Expected gswin64c.exe or gswin32c.exe.\n\n"
                        "Use it anyway?")
        );

        if (answer != QMessageBox::Yes) {
            return false;
        }
    }
#endif

    return true;
}

} // namespace

compress::compress(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::compress)
{
    ui->setupUi(this);

    QSettings settings;
    m_ghostscriptProgram = settings.value(
        QStringLiteral("compression/ghostscriptProgram")
    ).toString();

    if (!m_ghostscriptProgram.trimmed().isEmpty()) {
        ui->tool44->setToolTip(
            tr("Ghostscript executable:\n%1").arg(m_ghostscriptProgram)
        );
    }

    connect(ui->tool39, &QToolButton::clicked, this, [this]() {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Compression help"));
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText(tr("<b>Compression options</b>"));
        msgBox.setInformativeText(pageCompressHelpText1());
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    });

    connect(ui->tool42c, &QToolButton::clicked, this, [this]() {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Downsampling help"));
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText(tr("<b>Downsampling options</b>"));
        msgBox.setInformativeText(pageCompressHelpText2());
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    });

    connect(ui->tool43, &QToolButton::clicked, this, [this]() {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Downsampling help"));
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText(tr("<b>Downsampling options</b>"));
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

        removeEmptyRows(ui->tbl37, 0);

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

        for (int row : rows) {
            ui->tbl37->removeRow(row);
        }

        resetEmptyRows(ui->tbl37, 5);
    });

    connect(ui->btn38c, &QPushButton::clicked, this, [this]() {
        ui->tbl37->setRowCount(0);
        resetEmptyRows(ui->tbl37, 5);
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

        if (!validateGhostscriptExecutable(this, program)) {
            return;
        }

        const QFileInfo programInfo(program);
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

        const QStringList inputPdfPaths = getPdfPathsFromTableWidget(ui->tbl37, 0);

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

        CompressOptions options;
        options.inputPaths = inputPdfPaths;
        options.outputPath = outputPath;
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
        const OperationResult result = compressPdfs(options, &results);

        if (!result.ok) {
            QMessageBox::critical(
                this,
                tr("Compression failed"),
                result.message
            );
            return;
        }

        QMessageBox::information(
            this,
            tr("Compression complete"),
            makePdfCompressionReport(results)
        );
    });
}

compress::~compress()
{
    delete ui;
}
