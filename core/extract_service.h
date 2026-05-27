#ifndef EXTRACT_SERVICE_H
#define EXTRACT_SERVICE_H

#include "operation_result.h"

#include <QString>
#include <QStringList>

struct ExtractOptions
{
    enum class OutputMode
    {
        OnePdfPerRange,
        OnePdfPerInputFile,
        OnePdfForAllInputFiles
    };

    QStringList inputPaths;
    QString outputPath;
    QString pageRangeSpec;
    OutputMode outputMode = OutputMode::OnePdfPerRange;
};

OperationResult extractPdfs(
    const ExtractOptions& options,
    QStringList* createdFiles = nullptr);

#endif // EXTRACT_SERVICE_H
