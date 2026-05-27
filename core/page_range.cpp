#include "page_range.h"

#include <QStringList>

bool parseSingleDocumentPageSpec(
    const QString& input,
    DocumentPageSelection& selection,
    QString& error)
{
    const QString text = input.trimmed();

    selection = DocumentPageSelection{};
    error.clear();

    if (text.isEmpty() || text.compare("all", Qt::CaseInsensitive) == 0) {
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

QVector<DocumentPageSelection> parsePageSelections(
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

        for (DocumentPageSelection& selection : result) {
            selection.allPages = true;
        }

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

    for (int i = 0; i < documentCount; ++i) {
        result.append(sharedSelection);
    }

    return result;
}

bool validateSelectionsAgainstPageCounts(
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
            error = QString("Document %1 has no readable pages.").arg(docIndex + 1);
            return false;
        }

        if (selection.allPages) {
            continue;
        }

        for (const PageRange& range : selection.ranges) {
            if (range.first < 1 || range.last < 1) {
                error = QString("Document %1 contains an invalid page range %2-%3.")
                    .arg(docIndex + 1)
                    .arg(range.first)
                    .arg(range.last);
                return false;
            }

            if (range.first > range.last) {
                error = QString("Document %1 contains a descending page range %2-%3.")
                    .arg(docIndex + 1)
                    .arg(range.first)
                    .arg(range.last);
                return false;
            }

            if (range.first > pageCount) {
                error = QString("Document %1 has only %2 page(s), but the range starts at page %3.")
                    .arg(docIndex + 1)
                    .arg(pageCount)
                    .arg(range.first);
                return false;
            }

            if (range.last > pageCount) {
                error = QString("Document %1 has only %2 page(s), but the range ends at page %3.")
                    .arg(docIndex + 1)
                    .arg(pageCount)
                    .arg(range.last);
                return false;
            }
        }
    }

    return true;
}

int selectedPageCountForDocument(
    const DocumentPageSelection& selection,
    int pageCount)
{
    if (pageCount <= 0) {
        return 0;
    }

    if (selection.allPages) {
        return pageCount;
    }

    int count = 0;

    for (const PageRange& range : selection.ranges) {
        count += range.last - range.first + 1;
    }

    return count;
}

QString selectedPagesTextForDocument(
    const DocumentPageSelection& selection)
{
    if (selection.allPages) {
        return QStringLiteral("all pages");
    }

    QStringList parts;

    for (const PageRange& range : selection.ranges) {
        if (range.first == range.last) {
            parts << QString::number(range.first);
        } else {
            parts << QString("%1-%2").arg(range.first).arg(range.last);
        }
    }

    return parts.join(", ");
}
