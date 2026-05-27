#ifndef IMAGE_TO_PDF_SERVICE_H
#define IMAGE_TO_PDF_SERVICE_H

#include "operation_result.h"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QtGlobal>

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

    // check47: generate a clickable PDF table of contents page.
    bool generateHyperlinkedTableOfContents = false;

    ImageScaleMode scaleMode = ImageScaleMode::FitPage;
    bool preserveAspectRatio = true;
    bool centreImageOnPage = true;
    bool autoRotateFromMetadata = true;
    int pdfResolutionDpi = 300;
    int marginMillimetres = 10;
    QColor backgroundColour = QColor(Qt::white);
};

QString normaliseImagePdfOutputPath(const QString& outputPath);

OperationResult convertImagesToPdf(const ConvertImgOptions& options);

#endif // IMAGE_TO_PDF_SERVICE_H
