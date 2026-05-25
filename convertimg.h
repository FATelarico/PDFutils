#ifndef CONVERTIMG_H
#define CONVERTIMG_H

#include <QDialog>
#include <QColor>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QtGlobal>

class QTableWidget;

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

enum class ImageScaleMode
{
    FitPage,
    FillPageCrop,
    Stretch
};

struct ConvertImgOptions
{
    QStringList imagePaths;
    QString outputPdfPath;

    // check46: create one PDF page for each input image.
    bool oneImagePerPage = true;

    // check47: generate a clickable PDF table of contents/bookmarks.
    bool generateHyperlinkedTableOfContents = false;

    ImageScaleMode scaleMode = ImageScaleMode::FitPage;
    bool preserveAspectRatio = true;
    bool centreImageOnPage = true;
    bool autoRotateFromMetadata = true;
    int pdfResolutionDpi = 300;
    int marginMillimetres = 10;
    QColor backgroundColour = QColor(Qt::white);
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
