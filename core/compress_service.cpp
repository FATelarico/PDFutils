#include "compress_service.h"

#include "qpdf_utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <qpdf/QPDFJob.hh>

#include <algorithm>
#include <exception>
#include <vector>

namespace
{

QString formatCompressionByteSize(qint64 bytes)
{
    static const char* units[] = { "B", "KB", "MB", "GB", "TB" };

    if (bytes < 0) {
        bytes = 0;
    }

    double value = static_cast<double>(bytes);
    int unitIndex = 0;

    while (value >= 1024.0 && unitIndex < 4) {
        value /= 1024.0;
        ++unitIndex;
    }

    if (unitIndex == 0) {
        return QString::number(bytes) + " B";
    }

    return QString::number(value, 'f', 2) + ' ' + units[unitIndex];
}

QString cleanAbsoluteCompressionPath(const QString& path)
{
    const QFileInfo fileInfo(path.trimmed());
    return QDir::cleanPath(fileInfo.absoluteFilePath());
}

bool ensureCompressionOutputDirectoryExists(
    const QString& outputPath,
    QString& error)
{
    error.clear();

    const QFileInfo outputInfo(outputPath.trimmed());
    QDir outputDir = outputInfo.absoluteDir();

    if (outputDir.exists()) {
        return true;
    }

    if (!QDir().mkpath(outputDir.absolutePath())) {
        error = QString("Could not create output directory: %1")
            .arg(outputDir.absolutePath());
        return false;
    }

    return true;
}

QString normalisedPdfPath(const QString& path)
{
    return ensurePdfExtension(path);
}

bool makeBatchOutputPath(
    const QString& inputPdfPath,
    const QString& chosenOutputPath,
    int documentNumber,
    int documentCount,
    QString& outputPath,
    QString& error)
{
    outputPath.clear();
    error.clear();

    if (inputPdfPath.trimmed().isEmpty()) {
        error = "Input PDF path is empty.";
        return false;
    }

    if (chosenOutputPath.trimmed().isEmpty()) {
        error = "Output path is empty.";
        return false;
    }

    if (documentNumber <= 0 || documentCount <= 0 || documentNumber > documentCount) {
        error = QString("Invalid document numbering: document %1 of %2.")
            .arg(documentNumber)
            .arg(documentCount);
        return false;
    }

    if (documentCount == 1) {
        outputPath = normalisedPdfPath(chosenOutputPath);

        if (!ensureCompressionOutputDirectoryExists(outputPath, error)) {
            outputPath.clear();
            return false;
        }

        return true;
    }

    const QFileInfo inputInfo(inputPdfPath.trimmed());
    const QFileInfo chosenOutputInfo(normalisedPdfPath(chosenOutputPath));

    const QString inputBaseName = inputInfo.completeBaseName();
    const QString outputSuffix = chosenOutputInfo.completeBaseName();

    if (inputBaseName.trimmed().isEmpty()) {
        error = QString("Could not determine input file base name from: %1")
            .arg(inputPdfPath);
        return false;
    }

    if (outputSuffix.trimmed().isEmpty()) {
        error = QString("Could not determine output file suffix from: %1")
            .arg(chosenOutputPath);
        return false;
    }

    QDir outputDir = chosenOutputInfo.absoluteDir();

    if (!outputDir.exists() && !QDir().mkpath(outputDir.absolutePath())) {
        error = QString("Could not create output directory: %1")
            .arg(outputDir.absolutePath());
        return false;
    }

    const int digitWidth = QString::number(documentCount).length();
    const QString paddedDocumentNumber =
        QString("%1").arg(documentNumber, digitWidth, 10, QLatin1Char('0'));

    const QString outputFileName =
        QString("%1%2-%3.pdf")
            .arg(inputBaseName)
            .arg(outputSuffix)
            .arg(paddedDocumentNumber);

    outputPath = outputDir.filePath(outputFileName);
    return true;
}

QStringList ghostscriptProgramCandidates()
{
#ifdef Q_OS_WIN
    return QStringList()
        << "gswin64c.exe"
        << "gswin32c.exe"
        << "gs.exe"
        << "gswin64c"
        << "gswin32c"
        << "gs";
#else
    return QStringList() << "gs";
#endif
}

bool findGhostscriptProgram(
    const QString& configuredProgram,
    QString& ghostscriptProgram,
    QString& error)
{
    ghostscriptProgram.clear();
    error.clear();

    const QString trimmedProgram = configuredProgram.trimmed();

    if (!trimmedProgram.isEmpty()) {
        const QFileInfo configuredInfo(trimmedProgram);

        if (configuredInfo.isAbsolute()) {
            if (!configuredInfo.exists() || !configuredInfo.isFile()) {
                error = QString("Ghostscript executable was not found: %1")
                    .arg(trimmedProgram);
                return false;
            }

#ifndef Q_OS_WIN
            if (!configuredInfo.isExecutable()) {
                error = QString("Ghostscript executable is not executable: %1")
                    .arg(trimmedProgram);
                return false;
            }
#endif

            ghostscriptProgram = configuredInfo.absoluteFilePath();
            return true;
        }

        const QString foundConfigured = QStandardPaths::findExecutable(trimmedProgram);

        if (!foundConfigured.isEmpty()) {
            ghostscriptProgram = foundConfigured;
            return true;
        }

        error = QString("Ghostscript executable was not found in PATH: %1")
            .arg(trimmedProgram);
        return false;
    }

    const QStringList candidates = ghostscriptProgramCandidates();

    for (const QString& candidate : candidates) {
        const QString found = QStandardPaths::findExecutable(candidate);

        if (!found.isEmpty()) {
            ghostscriptProgram = found;
            return true;
        }
    }

#ifdef Q_OS_WIN
    const QStringList programDirs = {
        qEnvironmentVariable("ProgramFiles"),
        qEnvironmentVariable("ProgramFiles(x86)")
    };

    for (const QString& programDir : programDirs) {
        if (programDir.trimmed().isEmpty()) {
            continue;
        }

        QDir gsRoot(programDir + "/gs");

        if (!gsRoot.exists()) {
            continue;
        }

        const QFileInfoList versionDirs = gsRoot.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name | QDir::Reversed
        );

        for (const QFileInfo& versionDir : versionDirs) {
            const QString binPath = versionDir.absoluteFilePath() + "/bin";

            const QString gs64 = binPath + "/gswin64c.exe";
            if (QFileInfo::exists(gs64)) {
                ghostscriptProgram = QDir::cleanPath(gs64);
                return true;
            }

            const QString gs32 = binPath + "/gswin32c.exe";
            if (QFileInfo::exists(gs32)) {
                ghostscriptProgram = QDir::cleanPath(gs32);
                return true;
            }
        }
    }

    error = "Ghostscript was not found. Select gswin64c.exe/gswin32c.exe or add Ghostscript to PATH.";
#else
    error = "Ghostscript was not found. Install Ghostscript or add gs to PATH.";
#endif

    return false;
}

QStringList qpdfCompressionArguments(
    const QString& inputPdfPath,
    const QString& outputPdfPath,
    const CompressOptions& options)
{
    QStringList args;

    args << "qpdf";
    args << QDir::toNativeSeparators(QFileInfo(inputPdfPath).absoluteFilePath());

    args << "--compress-streams=y";
    args << "--decode-level=generalized";
    args << "--recompress-flate";
    args << "--compression-level=9";
    args << "--object-streams=generate";

    if (options.mode == PdfCompressionMode::StructureAndImages) {
        args << "--optimize-images";
    }

    if (options.removeMetadata) {
        args << "--remove-info";
        args << "--remove-metadata";
    }

    args << QDir::toNativeSeparators(QFileInfo(outputPdfPath).absoluteFilePath());

    return args;
}

bool runQpdfCompressionJob(
    const QString& inputPdfPath,
    const QString& outputPdfPath,
    const CompressOptions& options,
    QString& error)
{
    error.clear();

    const QStringList stringArgs = qpdfCompressionArguments(
        inputPdfPath,
        outputPdfPath,
        options
    );

    std::vector<QByteArray> argStorage;
    std::vector<char const*> argv;

    argStorage.reserve(static_cast<size_t>(stringArgs.size()));
    argv.reserve(static_cast<size_t>(stringArgs.size() + 1));

    for (const QString& arg : stringArgs) {
        argStorage.push_back(arg.toUtf8());
    }

    for (const QByteArray& arg : argStorage) {
        argv.push_back(arg.constData());
    }

    argv.push_back(nullptr);

    try {
        QPDFJob job;
        job.initializeFromArgv(argv.data());
        job.run();

        const int exitCode = job.getExitCode();

        if (exitCode != qpdf_exit_success && exitCode != qpdf_exit_warning) {
            error = QString("qpdf failed with exit code %1.").arg(exitCode);
            return false;
        }

        return true;
    }
    catch (const std::exception& e) {
        error = QString("qpdf compression failed: %1")
            .arg(QString::fromUtf8(e.what()));
        return false;
    }
}

bool createGhostscriptMetadataBlankingPdfmark(
    QTemporaryFile& pdfmarkFile,
    QString& error)
{
    error.clear();

    if (!pdfmarkFile.open()) {
        error = QString("Could not create temporary Ghostscript pdfmark file: %1")
            .arg(pdfmarkFile.errorString());
        return false;
    }

    const QByteArray pdfmark =
        "[\n"
        "  /Title ()\n"
        "  /Author ()\n"
        "  /Subject ()\n"
        "  /Keywords ()\n"
        "  /Creator ()\n"
        "  /Producer ()\n"
        "  /CreationDate ()\n"
        "  /ModDate ()\n"
        "  /DOCINFO pdfmark\n";

    if (pdfmarkFile.write(pdfmark) != pdfmark.size()) {
        error = QString("Could not write temporary Ghostscript pdfmark file: %1")
            .arg(pdfmarkFile.errorString());
        return false;
    }

    if (!pdfmarkFile.flush()) {
        error = QString("Could not flush temporary Ghostscript pdfmark file: %1")
            .arg(pdfmarkFile.errorString());
        return false;
    }

    pdfmarkFile.close();
    return true;
}

QStringList ghostscriptPresetCompressionArguments(
    const QString& inputPdfPath,
    const QString& outputPdfPath,
    const CompressOptions& options,
    const QString& pdfmarkPath = QString())
{
    QStringList args;

    args << "-dSAFER";
    args << "-dBATCH";
    args << "-dNOPAUSE";
    args << "-dQUIET";
    args << "-sDEVICE=pdfwrite";
    args << "-dCompatibilityLevel=" + options.ghostscriptCompatibilityLevel;

    if (!options.ghostscriptPdfSettings.trimmed().isEmpty()) {
        args << "-dPDFSETTINGS=" + options.ghostscriptPdfSettings.trimmed();
    }

    args << "-sOutputFile=" + QDir::toNativeSeparators(QFileInfo(outputPdfPath).absoluteFilePath());
    args << QDir::toNativeSeparators(QFileInfo(inputPdfPath).absoluteFilePath());

    if (!pdfmarkPath.trimmed().isEmpty()) {
        args << QDir::toNativeSeparators(QFileInfo(pdfmarkPath).absoluteFilePath());
    }

    return args;
}

QString ghostscriptBoolValue(bool value)
{
    return value ? "true" : "false";
}

QString ghostscriptDoubleValue(double value)
{
    return QLocale::c().toString(value, 'g', 15);
}

QStringList ghostscriptCustomDownsamplingArguments(
    const QString& inputPdfPath,
    const QString& outputPdfPath,
    const CompressOptions& options)
{
    QStringList args;

    args << "-o";
    args << QDir::toNativeSeparators(QFileInfo(outputPdfPath).absoluteFilePath());
    args << "-sDEVICE=pdfwrite";
    args << "-dDownsampleColorImages=" + ghostscriptBoolValue(options.downsampleColorImages);
    args << "-dDownsampleGrayImages=" + ghostscriptBoolValue(options.downsampleGrayImages);
    args << "-dDownsampleMonoImages=" + ghostscriptBoolValue(options.downsampleMonoImages);
    args << "-dColorImageResolution=" + QString::number(options.colorImageResolution);
    args << "-dGrayImageResolution=" + QString::number(options.grayImageResolution);
    args << "-dMonoImageResolution=" + QString::number(options.monoImageResolution);
    args << "-dColorImageDownsampleThreshold=" + ghostscriptDoubleValue(options.colorImageDownsampleThreshold);
    args << "-dGrayImageDownsampleThreshold=" + ghostscriptDoubleValue(options.grayImageDownsampleThreshold);
    args << "-dMonoImageDownsampleThreshold=" + ghostscriptDoubleValue(options.monoImageDownsampleThreshold);
    args << QDir::toNativeSeparators(QFileInfo(inputPdfPath).absoluteFilePath());

    return args;
}

bool runGhostscriptCompressionProcess(
    const QString& inputPdfPath,
    const QString& outputPdfPath,
    const CompressOptions& options,
    QString& error)
{
    error.clear();

    QString ghostscriptProgram;
    QString programError;

    if (!findGhostscriptProgram(
            options.ghostscriptProgram,
            ghostscriptProgram,
            programError)) {
        error = programError;
        return false;
    }

    QTemporaryFile metadataBlankingPdfmark(
        QDir::tempPath() + QDir::separator() + "pdf_metadata_XXXXXX.ps"
    );

    QString pdfmarkPath;

    if (options.removeMetadata &&
        options.ghostscriptMode == GhostscriptCompressionMode::Preset) {
        QString pdfmarkError;

        if (!createGhostscriptMetadataBlankingPdfmark(
                metadataBlankingPdfmark,
                pdfmarkError)) {
            error = pdfmarkError;
            return false;
        }

        pdfmarkPath = metadataBlankingPdfmark.fileName();
    }

    QStringList arguments;

    if (options.ghostscriptMode == GhostscriptCompressionMode::CustomDownsampling) {
        arguments = ghostscriptCustomDownsamplingArguments(
            inputPdfPath,
            outputPdfPath,
            options
        );
    }
    else {
        arguments = ghostscriptPresetCompressionArguments(
            inputPdfPath,
            outputPdfPath,
            options,
            pdfmarkPath
        );
    }

    QProcess process;
    process.setProgram(ghostscriptProgram);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);

    process.start();

    if (!process.waitForStarted()) {
        error = QString("Could not start Ghostscript: %1")
            .arg(process.errorString());
        return false;
    }

    if (!process.waitForFinished(-1)) {
        error = QString("Ghostscript did not finish: %1")
            .arg(process.errorString());
        process.kill();
        process.waitForFinished(3000);
        return false;
    }

    const QString processOutput = QString::fromLocal8Bit(process.readAll()).trimmed();

    if (process.exitStatus() != QProcess::NormalExit) {
        error = "Ghostscript crashed.";

        if (!processOutput.isEmpty()) {
            error += "\n" + processOutput;
        }

        return false;
    }

    if (process.exitCode() != 0) {
        error = QString("Ghostscript failed with exit code %1.")
            .arg(process.exitCode());

        if (!processOutput.isEmpty()) {
            error += "\n" + processOutput;
        }

        return false;
    }

    return true;
}

bool compressOnePdf(
    const QString& inputPdfPath,
    const QString& outputPdfPath,
    const CompressOptions& options,
    PdfCompressionResult& result,
    QString& error)
{
    result = PdfCompressionResult{};
    error.clear();

    const QString cleanInputPath = inputPdfPath.trimmed();
    const QString cleanOutputPath = outputPdfPath.trimmed();

    if (cleanInputPath.isEmpty()) {
        error = "Input PDF path is empty.";
        return false;
    }

    if (cleanOutputPath.isEmpty()) {
        error = "Output PDF path is empty.";
        return false;
    }

    QFileInfo inputInfo(cleanInputPath);

    if (!inputInfo.exists()) {
        error = QString("Input file does not exist: %1").arg(cleanInputPath);
        return false;
    }

    if (!inputInfo.isFile()) {
        error = QString("Input path is not a file: %1").arg(cleanInputPath);
        return false;
    }

    QString outputDirectoryError;

    if (!ensureCompressionOutputDirectoryExists(cleanOutputPath, outputDirectoryError)) {
        error = outputDirectoryError;
        return false;
    }

    const QString absoluteInputPath = cleanAbsoluteCompressionPath(cleanInputPath);
    const QString absoluteOutputPath = cleanAbsoluteCompressionPath(cleanOutputPath);

    if (absoluteInputPath == absoluteOutputPath) {
        error = QString("Output file must be different from the input file: %1")
            .arg(absoluteInputPath);
        return false;
    }

    result.inputPath = inputInfo.absoluteFilePath();
    result.outputPath = QFileInfo(cleanOutputPath).absoluteFilePath();
    result.beforeBytes = inputInfo.size();

    QString backendError;
    bool ok = false;

    switch (options.backend) {
    case PdfCompressionBackend::Qpdf:
        ok = runQpdfCompressionJob(
            result.inputPath,
            result.outputPath,
            options,
            backendError
        );
        break;

    case PdfCompressionBackend::Ghostscript:
        ok = runGhostscriptCompressionProcess(
            result.inputPath,
            result.outputPath,
            options,
            backendError
        );
        break;
    }

    if (!ok) {
        QFile::remove(result.outputPath);
        error = backendError;
        return false;
    }

    QFileInfo outputInfo(result.outputPath);

    if (!outputInfo.exists() || !outputInfo.isFile()) {
        error = QString("The compression backend did not create the output file: %1")
            .arg(result.outputPath);
        return false;
    }

    result.afterBytes = outputInfo.size();
    return true;
}

bool compressPdfsInternal(
    const CompressOptions& options,
    QVector<PdfCompressionResult>& results,
    QString& error)
{
    results.clear();
    error.clear();

    if (options.inputPaths.isEmpty()) {
        error = "No PDF files were provided.";
        return false;
    }

    if (options.outputPath.trimmed().isEmpty()) {
        error = "No output file was provided.";
        return false;
    }

    QSet<QString> plannedOutputPaths;
    const int documentCount = options.inputPaths.size();

    for (int docIndex = 0; docIndex < documentCount; ++docIndex) {
        QString outputPath;
        QString outputPathError;

        if (!makeBatchOutputPath(
                options.inputPaths[docIndex],
                options.outputPath,
                docIndex + 1,
                documentCount,
                outputPath,
                outputPathError)) {
            error = QString("Document %1: %2")
                .arg(docIndex + 1)
                .arg(outputPathError);
            return false;
        }

        const QString outputKey = cleanAbsoluteCompressionPath(outputPath);

        if (plannedOutputPaths.contains(outputKey)) {
            error = QString("Output filename collision: %1").arg(outputPath);
            return false;
        }

        plannedOutputPaths.insert(outputKey);
    }

    for (int docIndex = 0; docIndex < documentCount; ++docIndex) {
        QString outputPath;
        QString outputPathError;

        if (!makeBatchOutputPath(
                options.inputPaths[docIndex],
                options.outputPath,
                docIndex + 1,
                documentCount,
                outputPath,
                outputPathError)) {
            error = QString("Document %1: %2")
                .arg(docIndex + 1)
                .arg(outputPathError);
            return false;
        }

        PdfCompressionResult result;
        QString compressionError;

        if (!compressOnePdf(
                options.inputPaths[docIndex],
                outputPath,
                options,
                result,
                compressionError)) {
            error = QString("Document %1: %2")
                .arg(docIndex + 1)
                .arg(compressionError);
            results.clear();
            return false;
        }

        results.append(result);
    }

    return true;
}

} // namespace

QString ghostscriptPdfSettingsFromComboIndex(int comboIndex)
{
    switch (comboIndex) {
    case 0:
        return "/screen";
    case 1:
        return "/ebook";
    case 2:
        return "/default";
    case 3:
        return "/printer";
    case 4:
        return "/prepress";
    default:
        return "/default";
    }
}

QString ghostscriptCompatibilityLevelFromComboIndex(int comboIndex)
{
    switch (comboIndex) {
    case 0:
        return "1.4";
    case 1:
        return "1.5";
    case 2:
        return "1.6";
    case 3:
        return "1.7";
    default:
        return "1.7";
    }
}

QString makePdfCompressionReport(
    const QVector<PdfCompressionResult>& results)
{
    if (results.isEmpty()) {
        return "No PDF files were compressed.";
    }

    QStringList lines;

    qint64 totalBefore = 0;
    qint64 totalAfter = 0;
    bool anyReduction = false;

    lines << QString("Created %1 PDF file(s).").arg(results.size());
    lines << QString();

    for (const PdfCompressionResult& result : results) {
        totalBefore += result.beforeBytes;
        totalAfter += result.afterBytes;

        const qint64 difference = result.beforeBytes - result.afterBytes;
        const bool reduced = difference > 0;
        anyReduction = anyReduction || reduced;

        lines << QFileInfo(result.outputPath).fileName();
        lines << QString("  Before: %1").arg(formatCompressionByteSize(result.beforeBytes));
        lines << QString("  After:  %1").arg(formatCompressionByteSize(result.afterBytes));

        if (reduced) {
            const double percent = result.beforeBytes > 0
                ? (100.0 * static_cast<double>(difference) / static_cast<double>(result.beforeBytes))
                : 0.0;

            lines << QString("  Reduced by: %1 (%2%)")
                .arg(formatCompressionByteSize(difference))
                .arg(QString::number(percent, 'f', 1));
        }
        else if (difference == 0) {
            lines << "  No size reduction achieved.";
        }
        else {
            const qint64 increase = -difference;
            const double percent = result.beforeBytes > 0
                ? (100.0 * static_cast<double>(increase) / static_cast<double>(result.beforeBytes))
                : 0.0;

            lines << QString("  No size reduction achieved. Output is %1 larger (%2%).")
                .arg(formatCompressionByteSize(increase))
                .arg(QString::number(percent, 'f', 1));
        }

        lines << QString();
    }

    if (results.size() > 1) {
        lines << "Total";
        lines << QString("  Before: %1").arg(formatCompressionByteSize(totalBefore));
        lines << QString("  After:  %1").arg(formatCompressionByteSize(totalAfter));

        const qint64 totalDifference = totalBefore - totalAfter;

        if (totalDifference > 0) {
            const double percent = totalBefore > 0
                ? (100.0 * static_cast<double>(totalDifference) / static_cast<double>(totalBefore))
                : 0.0;

            lines << QString("  Reduced by: %1 (%2%)")
                .arg(formatCompressionByteSize(totalDifference))
                .arg(QString::number(percent, 'f', 1));
        }
        else if (totalDifference == 0) {
            lines << "  No size reduction achieved.";
        }
        else {
            const qint64 increase = -totalDifference;
            const double percent = totalBefore > 0
                ? (100.0 * static_cast<double>(increase) / static_cast<double>(totalBefore))
                : 0.0;

            lines << QString("  No size reduction achieved. Output is %1 larger (%2%).")
                .arg(formatCompressionByteSize(increase))
                .arg(QString::number(percent, 'f', 1));
        }
    }

    if (!anyReduction) {
        lines << QString();
        lines << "No size reduction achieved.";
    }

    return lines.join('\n').trimmed();
}

OperationResult compressPdfs(
    const CompressOptions& options,
    QVector<PdfCompressionResult>* results)
{
    QVector<PdfCompressionResult> localResults;
    QString error;

    if (!compressPdfsInternal(options, localResults, error)) {
        if (results) {
            results->clear();
        }
        return OperationResult::failure(error);
    }

    if (results) {
        *results = localResults;
    }

    return OperationResult::success(
        QString("Compressed %1 PDF file(s).").arg(localResults.size())
    );
}
