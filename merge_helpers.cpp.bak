#include <QString>
#include <QStringList>
#include <QVector>

struct PageRange
{
    int first = 0;  // 1-based, inclusive
    int last = 0;   // 1-based, inclusive
};

struct DocumentPageSelection
{
    bool allPages = true;
    QVector<PageRange> ranges;
};

static bool parseSingleDocumentPageSpec(
    const QString& input,
    DocumentPageSelection& selection,
    QString& error)
{
    const QString text = input.trimmed();

    selection = DocumentPageSelection{};

    if (text.isEmpty()) {
        selection.allPages = true;
        return true;
    }

    selection.allPages = false;

    const QStringList items = text.split(',', Qt::KeepEmptyParts);

    for (const QString& rawItem : items) {
        const QString item = rawItem.trimmed();

        if (item.isEmpty()) {
            error = "Empty page item. Check for repeated or trailing commas.";
            return false;
        }

        const QStringList bounds = item.split('-', Qt::KeepEmptyParts);

        if (bounds.size() == 1) {
            bool ok = false;
            const int page = bounds[0].trimmed().toInt(&ok);

            if (!ok || page <= 0) {
                error = QString("Invalid page number '%1'.").arg(item);
                return false;
            }

            selection.ranges.append(PageRange{ page, page });
        }
        else if (bounds.size() == 2) {
            bool okFirst = false;
            bool okLast = false;

            const int first = bounds[0].trimmed().toInt(&okFirst);
            const int last = bounds[1].trimmed().toInt(&okLast);

            if (!okFirst || !okLast || first <= 0 || last <= 0) {
                error = QString("Invalid page range '%1'.").arg(item);
                return false;
            }

            if (first > last) {
                error = QString("Descending page range '%1' is not allowed. Use '%2-%3' instead.")
                .arg(item)
                    .arg(last)
                    .arg(first);
                return false;
            }

            selection.ranges.append(PageRange{ first, last });
        }
        else {
            error = QString("Invalid page range '%1'.").arg(item);
            return false;
        }
    }

    return true;
}

static QVector<DocumentPageSelection> parsePageSelections(
    const QString& input,
    int documentCount,
    QString& error)
{
    QVector<DocumentPageSelection> result;
    error.clear();

    if (documentCount <= 0) {
        error = "No PDF documents were provided.";
        return result;
    }

    const QString text = input.trimmed();

    if (text.isEmpty()) {
        result.resize(documentCount);

        for (DocumentPageSelection& selection : result)
            selection.allPages = true;

        return result;
    }

    if (text.contains(';')) {
        const QStringList documentSpecs = text.split(';', Qt::KeepEmptyParts);

        if (documentSpecs.size() != documentCount) {
            error = QString(
                        "Invalid range! %1 set(s) of ranges were specified. But merge %2 documents, "
                        "using multiple sets of ranges requires exactly %2 ranges separated by %3 semicolon(s)."
                        "Click on the help button by the page-range box for further clarifications."
                        )
                        .arg(documentSpecs.size())
                        .arg(documentCount)
                        .arg(documentCount - 1);

            return result;
        }

        for (int i = 0; i < documentSpecs.size(); ++i) {
            DocumentPageSelection selection;

            QString localError;
            if (!parseSingleDocumentPageSpec(documentSpecs[i], selection, localError)) {
                error = QString("Document %1: %2").arg(i + 1).arg(localError);
                result.clear();
                return result;
            }

            result.append(selection);
        }

        return result;
    }

    DocumentPageSelection sharedSelection;

    QString localError;
    if (!parseSingleDocumentPageSpec(text, sharedSelection, localError)) {
        error = localError;
        return result;
    }

    for (int i = 0; i < documentCount; ++i)
        result.append(sharedSelection);

    return result;
}

#include <QDebug>

static void debugPrintSelections(const QVector<DocumentPageSelection>& selections)
{
    for (int doc = 0; doc < selections.size(); ++doc) {
        const DocumentPageSelection& selection = selections[doc];

        if (selection.allPages) {
            qDebug() << "Document" << doc + 1 << ": all pages";
            continue;
        }

        QStringList parts;

        for (const PageRange& range : selection.ranges) {
            if (range.first == range.last)
                parts << QString::number(range.first);
            else
                parts << QString("%1-%2").arg(range.first).arg(range.last);
        }

        qDebug() << "Document" << doc + 1 << ":" << parts.join(", ");
    }
}

#include <QTableWidget>
#include <QTableWidgetItem>
// #include <QStringList> // Already loaded
#include <QFileInfo>

static QStringList getPdfPathsFromTableWidget(
    QTableWidget* tableWidget,
    int pathColumn = 0)
{
    QStringList paths;

    if (!tableWidget)
        return paths;

    if (pathColumn < 0 || pathColumn >= tableWidget->columnCount())
        return paths;

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem* item = tableWidget->item(row, pathColumn);

        if (!item)
            continue;

        const QString path = item->text().trimmed();

        if (!path.isEmpty())
            paths.append(path);
    }

    return paths;
}

#include <QDir>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>

static bool getPdfPageCountWithLibQpdf(
    const QString& pdfPath,
    int& pageCount,
    QString& error)
{
    pageCount = 0;
    error.clear();

    const QString cleanPath = pdfPath.trimmed();

    if (cleanPath.isEmpty()) {
        error = "Empty PDF path.";
        return false;
    }

    QFileInfo fileInfo(cleanPath);

    if (!fileInfo.exists()) {
        error = QString("File does not exist: %1").arg(cleanPath);
        return false;
    }

    if (!fileInfo.isFile()) {
        error = QString("Path is not a file: %1").arg(cleanPath);
        return false;
    }

    try {
        QPDF pdf;

        /*
            qpdf expects char* / std::string filenames as byte strings.
            For Unicode filenames, qpdf expects UTF-8.
        */
        const QByteArray fileNameUtf8 =
            QDir::toNativeSeparators(fileInfo.absoluteFilePath()).toUtf8();

        qDebug() << "Reading PDF:" << fileInfo.absoluteFilePath();

        pdf.processFile(fileNameUtf8.constData());

        QPDFPageDocumentHelper pageHelper(pdf);
        const auto pages = pageHelper.getAllPages();

        pageCount = static_cast<int>(pages.size());

        qDebug() << "Page count:" << pageCount
                 << "for" << fileInfo.absoluteFilePath();

        if (pageCount <= 0) {
            error = QString("qpdf read zero pages from: %1")
            .arg(fileInfo.absoluteFilePath());
            return false;
        }

        return true;
    }
    catch (const std::exception& e) {
        error = QString("Could not read PDF '%1': %2").arg(cleanPath, QString::fromUtf8(e.what()));
        return false;
    }
}

static bool getPdfPageCountsWithLibQpdf(
    const QStringList& pdfPaths,
    QVector<int>& pageCounts,
    QString& error)
{
    pageCounts.clear();
    error.clear();

    if (pdfPaths.isEmpty()) {
        error = "No PDF paths were provided.";
        return false;
    }

    for (int i = 0; i < pdfPaths.size(); ++i) {
        int pageCount = 0;

        if (!getPdfPageCountWithLibQpdf(pdfPaths[i], pageCount, error)) {
            error = QString("Document %1: %2")
            .arg(i + 1)
                .arg(error);

            pageCounts.clear();
            return false;
        }

        pageCounts.append(pageCount);
    }

    qDebug() << "Final pageCounts vector:" << pageCounts;

    return true;
}

static bool validateSelectionsAgainstPageCounts(
    const QVector<DocumentPageSelection>& selections,
    const QVector<int>& pageCounts,
    QString& error)
{
    error.clear();

    if (selections.size() != pageCounts.size()) {
        error = QString(
                    "Internal error: there are %1 page selection(s), but %2 PDF page count(s)."
                    )
                    .arg(selections.size())
                    .arg(pageCounts.size());

        return false;
    }

    for (int docIndex = 0; docIndex < selections.size(); ++docIndex) {
        const DocumentPageSelection& selection = selections[docIndex];
        const int pageCount = pageCounts[docIndex];

        if (pageCount <= 0) {
            error = QString(
                        "Document %1 has no readable pages."
                        )
                        .arg(docIndex + 1);

            return false;
        }

        if (selection.allPages)
            continue;

        for (const PageRange& range : selection.ranges) {
            if (range.first < 1 || range.last < 1) {
                error = QString(
                            "Document %1 contains an invalid page range %2-%3."
                            )
                            .arg(docIndex + 1)
                            .arg(range.first)
                            .arg(range.last);

                return false;
            }

            if (range.first > range.last) {
                error = QString(
                            "Document %1 contains a descending page range %2-%3."
                            )
                            .arg(docIndex + 1)
                            .arg(range.first)
                            .arg(range.last);

                return false;
            }

            if (range.first > pageCount) {
                error = QString(
                            "Document %1 has only %2 page(s), but the range starts at page %3."
                            )
                            .arg(docIndex + 1)
                            .arg(pageCount)
                            .arg(range.first);

                return false;
            }

            if (range.last > pageCount) {
                error = QString(
                            "Document %1 has only %2 page(s), but the range ends at page %3."
                            )
                            .arg(docIndex + 1)
                            .arg(pageCount)
                            .arg(range.last);

                return false;
            }
        }
    }

    return true;
}


#include <QByteArray>
#include <qpdf/QPDFWriter.hh>

// qpdf-compatible file paths

static QByteArray toQpdfFileName(const QString& path)
{
    QFileInfo fileInfo(path.trimmed());

    const QString absolutePath =
        QDir::toNativeSeparators(fileInfo.absoluteFilePath());

    return absolutePath.toUtf8();
}

// Merger
static bool mergeSelectedPagesWithLibQpdf(
    const QStringList& pdfPaths,
    const QVector<DocumentPageSelection>& selections,
    const QString& outputPath,
    QString& error)
{
    error.clear();

    if (pdfPaths.isEmpty()) {
        error = "No PDF files were provided.";
        return false;
    }

    if (pdfPaths.size() != selections.size()) {
        error = QString(
                    "Internal error: there are %1 PDF file(s), but %2 page selection(s)."
                    )
                    .arg(pdfPaths.size())
                    .arg(selections.size());

        return false;
    }

    if (outputPath.trimmed().isEmpty()) {
        error = "No output file was provided.";
        return false;
    }

    QString finalOutputPath = outputPath.trimmed();
    // Normalise the output extension to create `file.pdf`, not `file`
    if (!finalOutputPath.endsWith(".pdf", Qt::CaseInsensitive)) {
        finalOutputPath += ".pdf";
    }

    try {
        QPDF outputPdf;
        outputPdf.emptyPDF();

        QPDFPageDocumentHelper outputPages(outputPdf);

        for (int docIndex = 0; docIndex < pdfPaths.size(); ++docIndex) {
            const QString inputPath = pdfPaths[docIndex].trimmed();
            const DocumentPageSelection& selection = selections[docIndex];

            QFileInfo inputInfo(inputPath);

            if (!inputInfo.exists()) {
                error = QString("Document %1 does not exist: %2")
                .arg(docIndex + 1)
                    .arg(inputPath);
                return false;
            }

            QPDF inputPdf;
            const QByteArray inputFileName = toQpdfFileName(inputPath);
            inputPdf.processFile(inputFileName.constData());
            // OLD: Ddo not rely on temporaries for qpdf filenames
            // inputPdf.processFile(toQpdfFileName(inputPath).constData());

            QPDFPageDocumentHelper inputPages(inputPdf);
            const std::vector<QPDFPageObjectHelper> pages =
                inputPages.getAllPages();

            if (pages.empty()) {
                error = QString("Document %1 has no readable pages: %2")
                .arg(docIndex + 1)
                    .arg(inputPath);
                return false;
            }

            if (selection.allPages) {
                for (const QPDFPageObjectHelper& page : pages) {
                    outputPages.addPage(page, false);
                }

                continue;
            }

            for (const PageRange& range : selection.ranges) {
                for (int pageNumber = range.first; pageNumber <= range.last; ++pageNumber) {
                    const int pageIndex = pageNumber - 1;

                    if (pageIndex < 0 || pageIndex >= static_cast<int>(pages.size())) {
                        error = QString(
                                    "Document %1 has %2 page(s), but page %3 was requested."
                                    )
                                    .arg(docIndex + 1)
                                    .arg(static_cast<int>(pages.size()))
                                    .arg(pageNumber);

                        return false;
                    }

                    outputPages.addPage(pages.at(static_cast<size_t>(pageIndex)), false);
                }
            }
        }

        const QByteArray outputFileName = toQpdfFileName(finalOutputPath);
        QPDFWriter writer(outputPdf, outputFileName.constData());
        writer.write();
        // OLD: Ddo not rely on temporaries for qpdf filenames
        // QPDFWriter writer(outputPdf, toQpdfFileName(outputPath).constData());
        // writer.write();

        return true;
    }
    catch (const std::exception& e) {
        error = QString("qpdf merge failed: %1")
        .arg(QString::fromUtf8(e.what()));
        return false;
    }
}

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

static QString formatFileSize(qint64 bytes)
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

static bool getPdfFileTableRowData(
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
    QString pageCountError;

    if (!getPdfPageCountWithLibQpdf(pdfPath, pageCount, pageCountError)) {
        error = QObject::tr("Could not read page count for %1: %2")
        .arg(pdfPath, pageCountError);
        return false;
    }

    rowData.path = fileInfo.absoluteFilePath();
    rowData.fileSizeBytes = fileInfo.size();
    rowData.pageCount = pageCount;

    return true;
}

static void setPdfFileTableRow(
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

static bool appendPdfFileToTableWidget(
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

static bool appendPdfFilesToTableWidget(
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