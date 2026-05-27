#ifndef PAGE_RANGE_H
#define PAGE_RANGE_H

#include <QString>
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

bool parseSingleDocumentPageSpec(
    const QString& input,
    DocumentPageSelection& selection,
    QString& error);

QVector<DocumentPageSelection> parsePageSelections(
    const QString& input,
    int documentCount,
    QString& error);

bool validateSelectionsAgainstPageCounts(
    const QVector<DocumentPageSelection>& selections,
    const QVector<int>& pageCounts,
    QString& error);

int selectedPageCountForDocument(
    const DocumentPageSelection& selection,
    int pageCount);

QString selectedPagesTextForDocument(
    const DocumentPageSelection& selection);

#endif // PAGE_RANGE_H
