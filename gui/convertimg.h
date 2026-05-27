#ifndef CONVERTIMG_H
#define CONVERTIMG_H

#include "image_to_pdf_service.h"

#include <QDialog>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace Ui {
class convertimg;
}

struct ImageTableColumns
{
    int path = 0;
    int fileSize = 1;
    int dimensions = 2;
};

struct ImageFileTableRowData
{
    QString path;
    qint64 fileSizeBytes = 0;
    QSize dimensions;
};

class convertimg : public QDialog
{
    Q_OBJECT

public:
    explicit convertimg(QWidget *parent = nullptr);
    ~convertimg();

private:
    Ui::convertimg *ui;

    static constexpr int PlaceholderRowCount = 7;

    void connectUiSignals();
    void configureImageTable();
    void resetEmptyImageRows();

    void addImageFiles();
    void removeSelectedImageRows();
    void clearImageTable();
    void runConversion();

    QStringList imagePathsFromTable() const;
    ConvertImgOptions collectOptions(const QString& outputPdfPath) const;

    bool hasImageRows() const;
};

#endif // CONVERTIMG_H
