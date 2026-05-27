#ifndef QPDF_UTILS_H
#define QPDF_UTILS_H

#include "operation_result.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

QByteArray toQpdfFileName(const QString& path);
QString ensurePdfExtension(const QString& outputPath);

OperationResult getPdfPageCount(
    const QString& pdfPath,
    int& pageCount);

OperationResult getPdfPageCounts(
    const QStringList& pdfPaths,
    QVector<int>& pageCounts);

#endif // QPDF_UTILS_H
