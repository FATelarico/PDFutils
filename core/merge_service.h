#ifndef MERGE_SERVICE_H
#define MERGE_SERVICE_H

#include "operation_result.h"

#include <QString>
#include <QStringList>

struct MergeOptions
{
    QStringList inputPaths;
    QString outputPath;
    QString pageRangeSpec;
    bool generateHyperlinkedToc = false;
};

OperationResult mergePdfs(const MergeOptions& options);

#endif // MERGE_SERVICE_H
