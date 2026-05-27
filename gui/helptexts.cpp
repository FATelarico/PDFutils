#include "helptexts.h"

#include <QObject>


QString pageRangeHelpText()
{
    return QObject::tr(
        "<b>Basic syntax:</b>"
        "<ul><li>Leave the field empty to use all pages.</li>"
        "<li>Use '2-6' to use pages 2 through 6, inclusive.</li>"
        "<li>Use '3,8' to use pages 3 and 8.</li>"
        "<li>Use '1-3,5,8-10' to use pages 1 through 3, page 5, "
        "and pages 8 through 10.</li></ul>"

        "<b>Multiple documents:</b><br>"
        "By default, the same page range is applied to every document.<br>"
        "To specify different ranges for different documents, separate each document's range with a semicolon (';').<br><br>"

        "<i>Examples:</i>"
        "<ul><li>Empty field: use all pages from all documents.</li>"
        "<li>'1-3,5': use pages 1 through 3 and page 5 from every document.</li>"
        "<li>'3;8': use page 3 from document 1 and page 8 from document 2.</li>"
        "<li>'1-3;': use pages 1 through 3 from document 1, and all pages "
        "from document 2.</li>"
        "<li>';1-3;': use all pages from document 1, pages 1 through 3 "
        "from document 2, and all pages from document 3.</li>"
        "<li>'1-3; ;' and '1-3;;': use pages 1 through 3 from document 1, "
        "and all pages from documents 2 and 3.</li></ul>"
    );
}

QString pageSplitHelpText1()
{
    return QObject::tr(
        "<u>Main splitting options:</u>"
        "<ul><li>Every page: Creates PDF files containing one page each</li>"
        "<li>Even pages: Creates PDF files containing two page each. The split will occur after page 2, 4, 6 etc</li>"
        "<li>Odd pages: Creates PDF files containing two page each. The split will occur after page 1, 3, 5 etc</li></ul>"
    );
}

QString pageSplitHelpText2()
{
    return QObject::tr(
        "<u>Meaning:</u><br>Set a comma separated list of page numbers after which to split"
    );
}

QString pageSplitHelpText3()
{
    return QObject::tr(
        "<u>Meaning:</u><br>Split every <i>n</i> pages"
    );
}


QString pageSplitHelpText4()
{
    return QObject::tr(
        "<u>Meaning:</u><br>Split a PDF file at the pages pointed by bookmarks at a"
        " certain depth level in the bookmarks tree."
    );
}


QString pageInsertHelpText1()
{
    return QObject::tr(
        "<u>Main insert options:</u>"
        "<ul><li>Every page: Insert after each page </li>"
        "<li>Even pages: Insert after page 2, 4, 6 etc</li>"
        "<li>Odd pages: Insert after page 1, 3, 5 etc</li></ul>"
    );
}


QString pageCompressHelpText1()
{
    return QObject::tr(
        "<u>Compression modes:</u>"
        "<ul><li>Safe: Uses built-in tools, to compress streams, recompress flate, "
        "improve PDF compression, regenerate object streams. This generally lose-less "
        "unless optionally image optimisation is employed.</li>"
        "<li>Aggressive: Uses an external executable, <a href=\"https://ghostscript.com/blog/optimizing-pdfs.html\">Ghostscript</a>, "
        "to recreate PDF files. This can reduce size dramatically, but it can also "
        "alter forms, annotations, metadata, colour handling, transparency, "
        "bookmarks, PDF version.</li></ul>"
    );
}


QString pageCompressHelpText2()
{
    return QObject::tr(
        "Choose how to downsample the images in a PDF file and reduce the number of pixels per square inch "
        "Usually A4/Letter size documents have a resolution of 150-300 dpi. A description of the most-commonly "
        "used options can be found"
        "<a href=\"https://ghostscript.readthedocs.io/en/latest/VectorDevices.html#distiller-parameters\">online<\a>."
    );
}
