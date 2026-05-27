#ifndef COMPRESS_SERVICE_H
#define COMPRESS_SERVICE_H

#include "operation_result.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <QtGlobal>

enum class PdfCompressionBackend
{
    Qpdf,
    Ghostscript
};

enum class PdfCompressionMode
{
    StructureOnly,
    StructureAndImages
};

enum class GhostscriptCompressionMode
{
    Preset,
    CustomDownsampling
};

struct CompressOptions
{
    QStringList inputPaths;
    QString outputPath;

    PdfCompressionBackend backend = PdfCompressionBackend::Qpdf;
    PdfCompressionMode mode = PdfCompressionMode::StructureOnly;
    bool removeMetadata = false;

    QString ghostscriptProgram;
    GhostscriptCompressionMode ghostscriptMode = GhostscriptCompressionMode::Preset;

    QString ghostscriptPdfSettings = "/default";
    QString ghostscriptCompatibilityLevel = "1.7";

    bool downsampleColorImages = false;
    bool downsampleGrayImages = false;
    bool downsampleMonoImages = false;
    int colorImageResolution = 150;
    int grayImageResolution = 150;
    int monoImageResolution = 300;
    double colorImageDownsampleThreshold = 1.5;
    double grayImageDownsampleThreshold = 1.5;
    double monoImageDownsampleThreshold = 1.5;
};

struct PdfCompressionResult
{
    QString inputPath;
    QString outputPath;
    qint64 beforeBytes = 0;
    qint64 afterBytes = 0;
};

QString ghostscriptPdfSettingsFromComboIndex(int comboIndex);
QString ghostscriptCompatibilityLevelFromComboIndex(int comboIndex);
QString makePdfCompressionReport(const QVector<PdfCompressionResult>& results);

OperationResult compressPdfs(
    const CompressOptions& options,
    QVector<PdfCompressionResult>* results = nullptr);

#endif // COMPRESS_SERVICE_H
