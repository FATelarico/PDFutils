#include "qpdf_utils.h"

#include <QDir>
#include <QFileInfo>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>

#include <exception>

QByteArray toQpdfFileName(const QString& path)
{
    QFileInfo fileInfo(path.trimmed());

    const QString absolutePath =
        QDir::toNativeSeparators(fileInfo.absoluteFilePath());

    return absolutePath.toUtf8();
}

QString ensurePdfExtension(const QString& outputPath)
{
    QString finalOutputPath = outputPath.trimmed();

    if (!finalOutputPath.endsWith(".pdf", Qt::CaseInsensitive)) {
        finalOutputPath += ".pdf";
    }

    return finalOutputPath;
}

OperationResult getPdfPageCount(
    const QString& pdfPath,
    int& pageCount)
{
    pageCount = 0;

    const QString cleanPath = pdfPath.trimmed();

    if (cleanPath.isEmpty()) {
        return OperationResult::failure("Empty PDF path.");
    }

    QFileInfo fileInfo(cleanPath);

    if (!fileInfo.exists()) {
        return OperationResult::failure(QString("File does not exist: %1").arg(cleanPath));
    }

    if (!fileInfo.isFile()) {
        return OperationResult::failure(QString("Path is not a file: %1").arg(cleanPath));
    }

    try {
        QPDF pdf;
        const QByteArray fileNameUtf8 = toQpdfFileName(fileInfo.absoluteFilePath());
        pdf.processFile(fileNameUtf8.constData());

        QPDFPageDocumentHelper pageHelper(pdf);
        const auto pages = pageHelper.getAllPages();

        pageCount = static_cast<int>(pages.size());

        if (pageCount <= 0) {
            return OperationResult::failure(
                QString("qpdf read zero pages from: %1")
                    .arg(fileInfo.absoluteFilePath())
            );
        }

        return OperationResult::success();
    }
    catch (const std::exception& e) {
        return OperationResult::failure(
            QString("Could not read PDF '%1': %2")
                .arg(cleanPath, QString::fromUtf8(e.what()))
        );
    }
}

OperationResult getPdfPageCounts(
    const QStringList& pdfPaths,
    QVector<int>& pageCounts)
{
    pageCounts.clear();

    if (pdfPaths.isEmpty()) {
        return OperationResult::failure("No PDF paths were provided.");
    }

    for (int i = 0; i < pdfPaths.size(); ++i) {
        int pageCount = 0;

        const OperationResult result = getPdfPageCount(pdfPaths[i], pageCount);

        if (!result.ok) {
            pageCounts.clear();
            return OperationResult::failure(
                QString("Document %1: %2")
                    .arg(i + 1)
                    .arg(result.message)
            );
        }

        pageCounts.append(pageCount);
    }

    return OperationResult::success();
}
