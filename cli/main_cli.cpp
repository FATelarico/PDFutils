#include "merge_service.h"
#include "split_service.h"
#include "extract_service.h"
#include "insert_service.h"
#include "compress_service.h"
#include "image_to_pdf_service.h"
#include "../updates/github_update_checker.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

/*
 * #ifndef APP_VERSION
 * #define APP_VERSION "0.3.0-alpha"
 * #endif
*/

namespace
{
    void printGeneralUsage(QTextStream& out)
    {
        out << "PDFutils-cli " << APP_VERSION << Qt::endl;
        out << "Usage:" << Qt::endl;
        out << "  PDFutils-cli merge [options] input1.pdf input2.pdf ..." << Qt::endl;
        out << "  PDFutils-cli split [options] input1.pdf input2.pdf ..." << Qt::endl;
        out << "  PDFutils-cli extract -o output_suffix.pdf [mode] [--pages spec] input1.pdf input2.pdf ..." << Qt::endl;
        out << "  PDFutils-cli insert [options] input1.pdf input2.pdf ..." << Qt::endl;
        out << "  PDFutils-cli compress -o output.pdf [options] input1.pdf input2.pdf ..." << Qt::endl;
        out << "  PDFutils-cli img2pdf -o output.pdf [options] image1 image2 ..." << Qt::endl;
        out << "  PDFutils-cli update-check [--prerelease] [--json] [--quiet]" << Qt::endl;
        out << Qt::endl;
        out << "Commands:" << Qt::endl;
        out << "  merge      Merge PDF files" << Qt::endl;
        out << "  split      Split PDF files" << Qt::endl;
        out << "  extract    Extract pages from PDF files" << Qt::endl;
        out << "  insert     Insert one PDF into other PDF files" << Qt::endl;
        out << "  compress   Compress PDF files" << Qt::endl;
        out << "  img2pdf    Convert image files to PDF" << Qt::endl;
        out << "  update-check  Check GitHub for a newer PDFutils release" << Qt::endl;
    }

    void printMergeUsage(QTextStream& out)
    {
        out << "Usage:" << Qt::endl;
        out << "  PDFutils-cli merge -o output.pdf [--pages spec] [--toc] input1.pdf input2.pdf ..." << Qt::endl;
        out << Qt::endl;
        out << "Options:" << Qt::endl;
        out << "  -o, --output <file>     Output PDF path" << Qt::endl;
        out << "  --pages <spec>          Page range spec. Examples: \"1-3\", \"1-2;all;4-6\"" << Qt::endl;
        out << "  --toc                   Generate a hyperlinked table of contents" << Qt::endl;
        out << "  -h, --help              Show merge help" << Qt::endl;
    }

    void printSplitUsage(QTextStream& out)
    {
        out << "Usage:" << Qt::endl;
        out << "  PDFutils-cli split -o output_suffix.pdf [mode] input1.pdf input2.pdf ..." << Qt::endl;
        out << Qt::endl;
        out << "Modes:" << Qt::endl;
        out << "  --single-pages          Create one output PDF per page. Default mode" << Qt::endl;
        out << "  --two-pages-even        Create two-page chunks: 1-2, 3-4, ..." << Qt::endl;
        out << "  --two-pages-odd         Split after odd pages: 1, 2-3, 4-5, ..." << Qt::endl;
        out << "  --points <spec>         Split after page numbers. Example: \"3,7\" or \"3,7;2,8\"" << Qt::endl;
        out << "  --every <n>             Split every n pages" << Qt::endl;
        out << "  --bookmarks <level>     Split at bookmarks of the requested level" << Qt::endl;
        out << Qt::endl;
        out << "Options:" << Qt::endl;
        out << "  -o, --output <file>     Output filename suffix/path. Example: /tmp/_split.pdf" << Qt::endl;
        out << "  -h, --help              Show split help" << Qt::endl;
    }

    void printExtractUsage(QTextStream& out)
    {
        out << "Usage:" << Qt::endl;
        out << "  PDFutils-cli extract -o output_suffix.pdf [mode] [--pages spec] input1.pdf input2.pdf ..." << Qt::endl;
        out << Qt::endl;
        out << "Modes:" << Qt::endl;
        out << "  --per-range             Create one PDF per selected range. Default mode" << Qt::endl;
        out << "  --per-file              Create one PDF per input file" << Qt::endl;
        out << "  --all                   Create one PDF containing all selected pages from all input files" << Qt::endl;
        out << Qt::endl;
        out << "Options:" << Qt::endl;
        out << "  -o, --output <file>     Output filename suffix/path. Example: /tmp/_extract.pdf" << Qt::endl;
        out << "  --pages <spec>          Page range spec. Examples: \"1-3\", \"1-2;all;4-6\"" << Qt::endl;
        out << "  -h, --help              Show extract help" << Qt::endl;
    }

    void printInsertUsage(QTextStream& out)
    {
        out << "Usage:" << Qt::endl;
        out << "  PDFutils-cli insert -i inserted.pdf -o output.pdf [mode] input1.pdf input2.pdf ..." << Qt::endl;
        out << Qt::endl;
        out << "Modes:" << Qt::endl;
        out << "  --after-every           Insert after every page. Default mode" << Qt::endl;
        out << "  --after-even            Insert after even pages" << Qt::endl;
        out << "  --after-odd             Insert after odd pages" << Qt::endl;
        out << "  --after-pages <spec>    Insert after listed pages. Example: \"3,7\" or \"3,7;2,8\"" << Qt::endl;
        out << "  --every <n>             Insert every n pages" << Qt::endl;
        out << "  --bookmarks <level>     Insert before bookmarks of the requested level" << Qt::endl;
        out << Qt::endl;
        out << "Options:" << Qt::endl;
        out << "  -i, --insert <file>     PDF whose pages will be inserted" << Qt::endl;
        out << "  -o, --output <file>     Output PDF path/suffix. Example: /tmp/_inserted.pdf" << Qt::endl;
        out << "  -h, --help              Show insert help" << Qt::endl;
    }

    void printCompressUsage(QTextStream& out)
    {
        out << "Usage:" << Qt::endl;
        out << "  PDFutils-cli compress -o output.pdf [backend] [options] input1.pdf input2.pdf ..." << Qt::endl;
        out << Qt::endl;
        out << "Backends:" << Qt::endl;
        out << "  --qpdf                  Use qpdf. Default backend" << Qt::endl;
        out << "  --ghostscript           Use Ghostscript" << Qt::endl;
        out << Qt::endl;
        out << "qpdf options:" << Qt::endl;
        out << "  --structure-only        Lossless structural optimisation. Default qpdf mode" << Qt::endl;
        out << "  --optimize-images       Enable qpdf image optimisation" << Qt::endl;
        out << Qt::endl;
        out << "Ghostscript preset options:" << Qt::endl;
        out << "  --gs-program <file>     Ghostscript executable path/name" << Qt::endl;
        out << "  --gs-preset <preset>    screen, ebook, default, printer, prepress" << Qt::endl;
        out << "  --gs-compat <version>   1.4, 1.5, 1.6, 1.7" << Qt::endl;
        out << Qt::endl;
        out << "Ghostscript custom downsampling:" << Qt::endl;
        out << "  --gs-custom             Use explicit downsampling parameters" << Qt::endl;
        out << "  --color-dpi <n>         Downsample colour images to n dpi" << Qt::endl;
        out << "  --gray-dpi <n>          Downsample greyscale images to n dpi" << Qt::endl;
        out << "  --mono-dpi <n>          Downsample monochrome images to n dpi" << Qt::endl;
        out << "  --color-threshold <n>   Colour image downsample threshold" << Qt::endl;
        out << "  --gray-threshold <n>    Greyscale image downsample threshold" << Qt::endl;
        out << "  --mono-threshold <n>    Monochrome image downsample threshold" << Qt::endl;
        out << Qt::endl;
        out << "General options:" << Qt::endl;
        out << "  -o, --output <file>     Output PDF path/suffix" << Qt::endl;
        out << "  --remove-metadata       Remove/blank metadata where supported" << Qt::endl;
        out << "  -h, --help              Show compress help" << Qt::endl;
    }

    void printImageToPdfUsage(QTextStream& out)
    {
        out << "Usage:" << Qt::endl;
        out << "  PDFutils-cli img2pdf -o output.pdf [options] image1 image2 ..." << Qt::endl;
        out << Qt::endl;
        out << "Options:" << Qt::endl;
        out << "  -o, --output <file>     Output PDF path" << Qt::endl;
        out << "  --toc                   Add a hyperlinked table of contents" << Qt::endl;
        out << "  --fit                   Fit each image inside the page. Default" << Qt::endl;
        out << "  --fill                  Fill the page and crop overflow" << Qt::endl;
        out << "  --stretch               Stretch image to page area" << Qt::endl;
        out << "  --dpi <n>               PDF render resolution. Default: 300" << Qt::endl;
        out << "  --margin-mm <n>         Page margin in millimetres. Default: 10" << Qt::endl;
        out << "  --no-centre             Do not centre fitted images" << Qt::endl;
        out << "  --no-auto-rotate        Ignore image orientation metadata" << Qt::endl;
        out << "  -h, --help              Show img2pdf help" << Qt::endl;
    }

    void printUpdateCheckUsage(QTextStream& out)
    {
        out << "Usage:" << Qt::endl;
        out << "  PDFutils-cli update-check [--prerelease] [--json] [--quiet]" << Qt::endl;
        out << Qt::endl;
        out << "Options:" << Qt::endl;
        out << "  --prerelease            Include GitHub pre-releases" << Qt::endl;
        out << "  --json                  Print machine-readable JSON" << Qt::endl;
        out << "  --quiet                 Print nothing; use exit status only" << Qt::endl;
        out << "  -h, --help              Show update-check help" << Qt::endl;
        out << Qt::endl;
        out << "Exit codes:" << Qt::endl;
        out << "  0                       Check succeeded; no update available" << Qt::endl;
        out << "  10                      Check succeeded; update available" << Qt::endl;
        out << "  1                       Network/API/parsing failure" << Qt::endl;
        out << "  2                       Invalid update-check usage" << Qt::endl;
    }

    /*
     * All `print*Command` should be above this line
     */


    // If no commands remain unimplemented, this function becomes dead code
    /*
     * int commandNotImplemented(const QString& command)
     * {
     *     QTextStream err(stderr);
     *     err << "The '" << command << "' command is recognised but not implemented yet." << Qt::endl;
     *     return 2;
     * }
    */

    /*
     * All `run*Command` should be below this line
     */

    int runMergeCommand(const QStringList& args)
    {
        QTextStream out(stdout);
        QTextStream err(stderr);

        MergeOptions options;

        for (int i = 0; i < args.size(); ++i) {
            const QString arg = args.at(i);

            if (arg == "-h" || arg == "--help") {
                printMergeUsage(out);
                return 0;
            }

            if (arg == "--toc") {
                options.generateHyperlinkedToc = true;
                continue;
            }

            if (arg == "-o" || arg == "--output") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after " << arg << Qt::endl;
                    return 1;
                }

                options.outputPath = args.at(++i);
                continue;
            }

            if (arg == "--pages") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --pages" << Qt::endl;
                    return 1;
                }

                options.pageRangeSpec = args.at(++i);
                continue;
            }

            if (arg.startsWith('-')) {
                err << "Unknown merge option: " << arg << Qt::endl;
                printMergeUsage(err);
                return 1;
            }

            options.inputPaths.append(arg);
        }

        if (options.inputPaths.isEmpty()) {
            err << "No input PDF files were provided." << Qt::endl;
            printMergeUsage(err);
            return 1;
        }

        if (options.outputPath.trimmed().isEmpty()) {
            err << "No output PDF path was provided." << Qt::endl;
            printMergeUsage(err);
            return 1;
        }

        const OperationResult result = mergePdfs(options);

        if (!result.ok) {
            err << result.message << Qt::endl;
            return 1;
        }

        out << result.message << Qt::endl;
        return 0;
    }

    int parsePositiveIntegerOption(
        const QString& optionName,
        const QString& valueText,
        int& value)
    {
        QTextStream err(stderr);

        bool ok = false;
        const int parsed = valueText.toInt(&ok);

        if (!ok || parsed <= 0) {
            err << "Invalid value for " << optionName << ": " << valueText << Qt::endl;
            return 1;
        }

        value = parsed;
        return 0;
    }

    int parsePositiveDoubleOption(
        const QString& optionName,
        const QString& valueText,
        double& value)
    {
        QTextStream err(stderr);

        bool ok = false;
        const double parsed = valueText.toDouble(&ok);

        if (!ok || parsed <= 0.0) {
            err << "Invalid value for " << optionName << ": " << valueText << Qt::endl;
            return 1;
        }

        value = parsed;
        return 0;
    }

    int runSplitCommand(const QStringList& args)
    {
        QTextStream out(stdout);
        QTextStream err(stderr);

        SplitOptions options;

        bool modeWasSet = false;

        for (int i = 0; i < args.size(); ++i) {
            const QString arg = args.at(i);

            if (arg == "-h" || arg == "--help") {
                printSplitUsage(out);
                return 0;
            }

            if (arg == "-o" || arg == "--output") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after " << arg << Qt::endl;
                    return 1;
                }

                options.outputPath = args.at(++i);
                continue;
            }

            if (arg == "--single-pages") {
                options.mode = SplitOptions::Mode::Fixed;
                options.fixedModeIndex = 0;
                modeWasSet = true;
                continue;
            }

            if (arg == "--two-pages-even") {
                options.mode = SplitOptions::Mode::Fixed;
                options.fixedModeIndex = 1;
                modeWasSet = true;
                continue;
            }

            if (arg == "--two-pages-odd") {
                options.mode = SplitOptions::Mode::Fixed;
                options.fixedModeIndex = 2;
                modeWasSet = true;
                continue;
            }

            if (arg == "--points") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --points" << Qt::endl;
                    return 1;
                }

                options.mode = SplitOptions::Mode::ManualSplitPoints;
                options.manualSplitPoints = args.at(++i);
                modeWasSet = true;
                continue;
            }

            if (arg == "--every") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --every" << Qt::endl;
                    return 1;
                }

                int pagesPerPart = 0;
                if (parsePositiveIntegerOption("--every", args.at(++i), pagesPerPart) != 0) {
                    return 1;
                }

                options.mode = SplitOptions::Mode::EveryNPages;
                options.pagesPerPart = pagesPerPart;
                modeWasSet = true;
                continue;
            }

            if (arg == "--bookmarks") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --bookmarks" << Qt::endl;
                    return 1;
                }

                int bookmarkLevel = 0;
                if (parsePositiveIntegerOption("--bookmarks", args.at(++i), bookmarkLevel) != 0) {
                    return 1;
                }

                options.mode = SplitOptions::Mode::Bookmarks;
                options.bookmarkLevel = bookmarkLevel;
                modeWasSet = true;
                continue;
            }

            if (arg.startsWith('-')) {
                err << "Unknown split option: " << arg << Qt::endl;
                printSplitUsage(err);
                return 1;
            }

            options.inputPaths.append(arg);
        }

        if (!modeWasSet) {
            options.mode = SplitOptions::Mode::Fixed;
            options.fixedModeIndex = 0;
        }

        if (options.inputPaths.isEmpty()) {
            err << "No input PDF files were provided." << Qt::endl;
            printSplitUsage(err);
            return 1;
        }

        if (options.outputPath.trimmed().isEmpty()) {
            err << "No output path was provided." << Qt::endl;
            printSplitUsage(err);
            return 1;
        }

        QStringList createdFiles;
        const OperationResult result = splitPdfs(options, &createdFiles);

        if (!result.ok) {
            err << result.message << Qt::endl;
            return 1;
        }

        out << result.message << Qt::endl;

        const QStringList& constCreatedFiles = createdFiles;

        for (const QString& createdFile : constCreatedFiles) {
            out << createdFile << Qt::endl;
        }

        return 0;
    }

    int runExtractCommand(const QStringList& args)
    {
        QTextStream out(stdout);
        QTextStream err(stderr);

        ExtractOptions options;
        bool modeWasSet = false;

        for (int i = 0; i < args.size(); ++i) {
            const QString arg = args.at(i);

            if (arg == "-h" || arg == "--help") {
                printExtractUsage(out);
                return 0;
            }

            if (arg == "-o" || arg == "--output") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after " << arg << Qt::endl;
                    return 1;
                }

                options.outputPath = args.at(++i);
                continue;
            }

            if (arg == "--pages") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --pages" << Qt::endl;
                    return 1;
                }

                options.pageRangeSpec = args.at(++i);
                continue;
            }

            if (arg == "--per-range") {
                options.outputMode = ExtractOptions::OutputMode::OnePdfPerRange;
                modeWasSet = true;
                continue;
            }

            if (arg == "--per-file") {
                options.outputMode = ExtractOptions::OutputMode::OnePdfPerInputFile;
                modeWasSet = true;
                continue;
            }

            if (arg == "--all") {
                options.outputMode = ExtractOptions::OutputMode::OnePdfForAllInputFiles;
                modeWasSet = true;
                continue;
            }

            if (arg.startsWith('-')) {
                err << "Unknown extract option: " << arg << Qt::endl;
                printExtractUsage(err);
                return 1;
            }

            options.inputPaths.append(arg);
        }

        if (!modeWasSet) {
            options.outputMode = ExtractOptions::OutputMode::OnePdfPerRange;
        }

        if (options.inputPaths.isEmpty()) {
            err << "No input PDF files were provided." << Qt::endl;
            printExtractUsage(err);
            return 1;
        }

        if (options.outputPath.trimmed().isEmpty()) {
            err << "No output path was provided." << Qt::endl;
            printExtractUsage(err);
            return 1;
        }

        QStringList createdFiles;
        const OperationResult result = extractPdfs(options, &createdFiles);

        if (!result.ok) {
            err << result.message << Qt::endl;
            return 1;
        }

        out << result.message << Qt::endl;

        const QStringList& constCreatedFiles = createdFiles;

        for (const QString& createdFile : constCreatedFiles) {
            out << createdFile << Qt::endl;
        }

        return 0;
    }

    int runInsertCommand(const QStringList& args)
    {
        QTextStream out(stdout);
        QTextStream err(stderr);

        InsertOptions options;
        bool modeWasSet = false;

        for (int i = 0; i < args.size(); ++i) {
            const QString arg = args.at(i);

            if (arg == "-h" || arg == "--help") {
                printInsertUsage(out);
                return 0;
            }

            if (arg == "-i" || arg == "--insert") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after " << arg << Qt::endl;
                    return 1;
                }

                options.insertedPdfPath = args.at(++i);
                continue;
            }

            if (arg == "-o" || arg == "--output") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after " << arg << Qt::endl;
                    return 1;
                }

                options.outputPath = args.at(++i);
                continue;
            }

            if (arg == "--after-every") {
                options.mode = InsertOptions::Mode::Fixed;
                options.fixedModeIndex = 0;
                modeWasSet = true;
                continue;
            }

            if (arg == "--after-even") {
                options.mode = InsertOptions::Mode::Fixed;
                options.fixedModeIndex = 1;
                modeWasSet = true;
                continue;
            }

            if (arg == "--after-odd") {
                options.mode = InsertOptions::Mode::Fixed;
                options.fixedModeIndex = 2;
                modeWasSet = true;
                continue;
            }

            if (arg == "--after-pages") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --after-pages" << Qt::endl;
                    return 1;
                }

                options.mode = InsertOptions::Mode::ManualPages;
                options.manualInsertionPages = args.at(++i);
                modeWasSet = true;
                continue;
            }

            if (arg == "--every") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --every" << Qt::endl;
                    return 1;
                }

                int pagesPerInsertion = 0;
                if (parsePositiveIntegerOption("--every", args.at(++i), pagesPerInsertion) != 0) {
                    return 1;
                }

                options.mode = InsertOptions::Mode::EveryNPages;
                options.pagesPerInsertion = pagesPerInsertion;
                modeWasSet = true;
                continue;
            }

            if (arg == "--bookmarks") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --bookmarks" << Qt::endl;
                    return 1;
                }

                int bookmarkLevel = 0;
                if (parsePositiveIntegerOption("--bookmarks", args.at(++i), bookmarkLevel) != 0) {
                    return 1;
                }

                options.mode = InsertOptions::Mode::Bookmarks;
                options.bookmarkLevel = bookmarkLevel;
                modeWasSet = true;
                continue;
            }

            if (arg.startsWith('-')) {
                err << "Unknown insert option: " << arg << Qt::endl;
                printInsertUsage(err);
                return 1;
            }

            options.inputPaths.append(arg);
        }

        if (!modeWasSet) {
            options.mode = InsertOptions::Mode::Fixed;
            options.fixedModeIndex = 0;
        }

        if (options.inputPaths.isEmpty()) {
            err << "No input PDF files were provided." << Qt::endl;
            printInsertUsage(err);
            return 1;
        }

        if (options.insertedPdfPath.trimmed().isEmpty()) {
            err << "No inserted PDF was provided." << Qt::endl;
            printInsertUsage(err);
            return 1;
        }

        if (options.outputPath.trimmed().isEmpty()) {
            err << "No output path was provided." << Qt::endl;
            printInsertUsage(err);
            return 1;
        }

        QStringList createdFiles;
        const OperationResult result = insertPagesIntoPdfs(options, &createdFiles);

        if (!result.ok) {
            err << result.message << Qt::endl;
            return 1;
        }

        out << result.message << Qt::endl;

        const QStringList& constCreatedFiles = createdFiles;

        for (const QString& createdFile : constCreatedFiles) {
            out << createdFile << Qt::endl;
        }

        return 0;
    }

    bool setGhostscriptPresetFromCli(
        const QString& preset,
        CompressOptions& options)
    {
        const QString value = preset.trimmed().toLower();

        if (value == "screen") {
            options.ghostscriptPdfSettings = "/screen";
            return true;
        }

        if (value == "ebook") {
            options.ghostscriptPdfSettings = "/ebook";
            return true;
        }

        if (value == "default") {
            options.ghostscriptPdfSettings = "/default";
            return true;
        }

        if (value == "printer") {
            options.ghostscriptPdfSettings = "/printer";
            return true;
        }

        if (value == "prepress") {
            options.ghostscriptPdfSettings = "/prepress";
            return true;
        }

        return false;
    }

    int runCompressCommand(const QStringList& args)
    {
        QTextStream out(stdout);
        QTextStream err(stderr);

        CompressOptions options;
        options.backend = PdfCompressionBackend::Qpdf;
        options.mode = PdfCompressionMode::StructureOnly;

        for (int i = 0; i < args.size(); ++i) {
            const QString arg = args.at(i);

            if (arg == "-h" || arg == "--help") {
                printCompressUsage(out);
                return 0;
            }

            if (arg == "-o" || arg == "--output") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after " << arg << Qt::endl;
                    return 1;
                }

                options.outputPath = args.at(++i);
                continue;
            }

            if (arg == "--qpdf") {
                options.backend = PdfCompressionBackend::Qpdf;
                continue;
            }

            if (arg == "--ghostscript") {
                options.backend = PdfCompressionBackend::Ghostscript;
                options.ghostscriptMode = GhostscriptCompressionMode::Preset;
                continue;
            }

            if (arg == "--structure-only") {
                options.backend = PdfCompressionBackend::Qpdf;
                options.mode = PdfCompressionMode::StructureOnly;
                continue;
            }

            if (arg == "--optimize-images") {
                options.backend = PdfCompressionBackend::Qpdf;
                options.mode = PdfCompressionMode::StructureAndImages;
                continue;
            }

            if (arg == "--remove-metadata") {
                options.removeMetadata = true;
                continue;
            }

            if (arg == "--gs-program") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --gs-program" << Qt::endl;
                    return 1;
                }

                options.backend = PdfCompressionBackend::Ghostscript;
                options.ghostscriptProgram = args.at(++i);
                continue;
            }

            if (arg == "--gs-preset") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --gs-preset" << Qt::endl;
                    return 1;
                }

                options.backend = PdfCompressionBackend::Ghostscript;
                options.ghostscriptMode = GhostscriptCompressionMode::Preset;

                const QString preset = args.at(++i);

                if (!setGhostscriptPresetFromCli(preset, options)) {
                    err << "Invalid Ghostscript preset: " << preset << Qt::endl;
                    printCompressUsage(err);
                    return 1;
                }

                continue;
            }

            if (arg == "--gs-compat") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --gs-compat" << Qt::endl;
                    return 1;
                }

                const QString compatibility = args.at(++i);

                if (compatibility != "1.4" &&
                    compatibility != "1.5" &&
                    compatibility != "1.6" &&
                    compatibility != "1.7") {
                    err << "Invalid Ghostscript compatibility level: " << compatibility << Qt::endl;
                    return 1;
                }

                options.backend = PdfCompressionBackend::Ghostscript;
                options.ghostscriptCompatibilityLevel = compatibility;
                continue;
            }

            if (arg == "--gs-custom") {
                options.backend = PdfCompressionBackend::Ghostscript;
                options.ghostscriptMode = GhostscriptCompressionMode::CustomDownsampling;
                continue;
            }

            if (arg == "--color-dpi") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --color-dpi" << Qt::endl;
                    return 1;
                }

                int dpi = 0;
                if (parsePositiveIntegerOption("--color-dpi", args.at(++i), dpi) != 0) {
                    return 1;
                }

                options.backend = PdfCompressionBackend::Ghostscript;
                options.ghostscriptMode = GhostscriptCompressionMode::CustomDownsampling;
                options.downsampleColorImages = true;
                options.colorImageResolution = dpi;
                continue;
            }

            if (arg == "--gray-dpi") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --gray-dpi" << Qt::endl;
                    return 1;
                }

                int dpi = 0;
                if (parsePositiveIntegerOption("--gray-dpi", args.at(++i), dpi) != 0) {
                    return 1;
                }

                options.backend = PdfCompressionBackend::Ghostscript;
                options.ghostscriptMode = GhostscriptCompressionMode::CustomDownsampling;
                options.downsampleGrayImages = true;
                options.grayImageResolution = dpi;
                continue;
            }

            if (arg == "--mono-dpi") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --mono-dpi" << Qt::endl;
                    return 1;
                }

                int dpi = 0;
                if (parsePositiveIntegerOption("--mono-dpi", args.at(++i), dpi) != 0) {
                    return 1;
                }

                options.backend = PdfCompressionBackend::Ghostscript;
                options.ghostscriptMode = GhostscriptCompressionMode::CustomDownsampling;
                options.downsampleMonoImages = true;
                options.monoImageResolution = dpi;
                continue;
            }

            if (arg == "--color-threshold") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --color-threshold" << Qt::endl;
                    return 1;
                }

                double threshold = 0.0;
                if (parsePositiveDoubleOption("--color-threshold", args.at(++i), threshold) != 0) {
                    return 1;
                }

                options.colorImageDownsampleThreshold = threshold;
                continue;
            }

            if (arg == "--gray-threshold") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --gray-threshold" << Qt::endl;
                    return 1;
                }

                double threshold = 0.0;
                if (parsePositiveDoubleOption("--gray-threshold", args.at(++i), threshold) != 0) {
                    return 1;
                }

                options.grayImageDownsampleThreshold = threshold;
                continue;
            }

            if (arg == "--mono-threshold") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --mono-threshold" << Qt::endl;
                    return 1;
                }

                double threshold = 0.0;
                if (parsePositiveDoubleOption("--mono-threshold", args.at(++i), threshold) != 0) {
                    return 1;
                }

                options.monoImageDownsampleThreshold = threshold;
                continue;
            }

            if (arg.startsWith('-')) {
                err << "Unknown compress option: " << arg << Qt::endl;
                printCompressUsage(err);
                return 1;
            }

            options.inputPaths.append(arg);
        }

        if (options.inputPaths.isEmpty()) {
            err << "No input PDF files were provided." << Qt::endl;
            printCompressUsage(err);
            return 1;
        }

        if (options.outputPath.trimmed().isEmpty()) {
            err << "No output path was provided." << Qt::endl;
            printCompressUsage(err);
            return 1;
        }

        QVector<PdfCompressionResult> results;
        const OperationResult result = compressPdfs(options, &results);

        if (!result.ok) {
            err << result.message << Qt::endl;
            return 1;
        }

        out << makePdfCompressionReport(results) << Qt::endl;
        return 0;
    }

    int runImageToPdfCommand(const QStringList& args)
    {
        QTextStream out(stdout);
        QTextStream err(stderr);

        ConvertImgOptions options;
        options.oneImagePerPage = true;

        for (int i = 0; i < args.size(); ++i) {
            const QString arg = args.at(i);

            if (arg == "-h" || arg == "--help") {
                printImageToPdfUsage(out);
                return 0;
            }

            if (arg == "-o" || arg == "--output") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after " << arg << Qt::endl;
                    return 1;
                }

                options.outputPdfPath = args.at(++i);
                continue;
            }

            if (arg == "--toc") {
                options.generateHyperlinkedTableOfContents = true;
                options.oneImagePerPage = true;
                continue;
            }

            if (arg == "--fit") {
                options.scaleMode = ImageScaleMode::FitPage;
                options.preserveAspectRatio = true;
                continue;
            }

            if (arg == "--fill") {
                options.scaleMode = ImageScaleMode::FillPageCrop;
                options.preserveAspectRatio = true;
                continue;
            }

            if (arg == "--stretch") {
                options.scaleMode = ImageScaleMode::Stretch;
                options.preserveAspectRatio = false;
                continue;
            }

            if (arg == "--dpi") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --dpi" << Qt::endl;
                    return 1;
                }

                int dpi = 0;
                if (parsePositiveIntegerOption("--dpi", args.at(++i), dpi) != 0) {
                    return 1;
                }

                options.pdfResolutionDpi = dpi;
                continue;
            }

            if (arg == "--margin-mm") {
                if (i + 1 >= args.size()) {
                    err << "Missing value after --margin-mm" << Qt::endl;
                    return 1;
                }

                int margin = 0;
                if (parsePositiveIntegerOption("--margin-mm", args.at(++i), margin) != 0) {
                    return 1;
                }

                options.marginMillimetres = margin;
                continue;
            }

            if (arg == "--no-centre") {
                options.centreImageOnPage = false;
                continue;
            }

            if (arg == "--no-auto-rotate") {
                options.autoRotateFromMetadata = false;
                continue;
            }

            if (arg.startsWith('-')) {
                err << "Unknown img2pdf option: " << arg << Qt::endl;
                printImageToPdfUsage(err);
                return 1;
            }

            options.imagePaths.append(arg);
        }

        if (options.imagePaths.isEmpty()) {
            err << "No input image files were provided." << Qt::endl;
            printImageToPdfUsage(err);
            return 1;
        }

        if (options.outputPdfPath.trimmed().isEmpty()) {
            err << "No output PDF path was provided." << Qt::endl;
            printImageToPdfUsage(err);
            return 1;
        }

        const OperationResult result = convertImagesToPdf(options);

        if (!result.ok) {
            err << result.message << Qt::endl;
            return 1;
        }

        out << result.message << Qt::endl;
        return 0;
    }

    QString releaseTypeText(const GitHubReleaseInfo& release)
    {
        return release.prerelease
                   ? QStringLiteral("pre-release")
                   : QStringLiteral("full release");
    }

    QString isoDateOrUnknown(const QDateTime& dateTime)
    {
        if (!dateTime.isValid())
            return QStringLiteral("unknown");

        return dateTime.toUTC().date().toString(Qt::ISODate);
    }

    void printUpdateCheckText(
        QTextStream& out,
        const GitHubUpdateCheckResult& result)
    {
        out << "Current version: " << result.currentVersion << Qt::endl;
        out << "Current release date: "
            << isoDateOrUnknown(result.currentReleaseDate);

        if (result.currentReleaseDateFromApi)
            out << " (GitHub API)";

        out << Qt::endl;

        out << "Latest release: " << result.latestRelease.tag << Qt::endl;
        out << "Latest release type: " << releaseTypeText(result.latestRelease) << Qt::endl;
        out << "Latest release date: "
            << isoDateOrUnknown(result.latestRelease.publishedAt) << Qt::endl;
        out << "Release URL: " << result.latestReleaseUrl.toString() << Qt::endl;
        out << Qt::endl;

        if (result.updateAvailable) {
            out << "Update available." << Qt::endl;
        } else {
            out << "PDFutils-cli is up to date." << Qt::endl;
        }
    }

    QJsonObject releaseToJsonObject(const GitHubReleaseInfo& release)
    {
        QJsonObject object;

        object.insert(QStringLiteral("tag"), release.tag);
        object.insert(QStringLiteral("prerelease"), release.prerelease);
        object.insert(
            QStringLiteral("publishedAt"),
            release.publishedAt.isValid()
                ? release.publishedAt.toUTC().toString(Qt::ISODate)
                : QString()
            );
        object.insert(QStringLiteral("htmlUrl"), release.htmlUrl);

        return object;
    }

    void printUpdateCheckJson(
        QTextStream& out,
        const GitHubUpdateCheckResult& result)
    {
        QJsonObject object;

        object.insert(QStringLiteral("ok"), result.ok);
        object.insert(QStringLiteral("owner"), result.owner);
        object.insert(QStringLiteral("repo"), result.repo);
        object.insert(QStringLiteral("currentVersion"), result.currentVersion);
        object.insert(
            QStringLiteral("currentReleaseDate"),
            result.currentReleaseDate.isValid()
                ? result.currentReleaseDate.toUTC().toString(Qt::ISODate)
                : QString()
            );
        object.insert(
            QStringLiteral("currentReleaseDateFromApi"),
            result.currentReleaseDateFromApi
            );
        object.insert(QStringLiteral("updateAvailable"), result.updateAvailable);
        object.insert(QStringLiteral("httpStatus"), result.httpStatus);
        object.insert(QStringLiteral("apiUrl"), result.apiUrl.toString());
        object.insert(QStringLiteral("latestReleaseUrl"), result.latestReleaseUrl.toString());
        object.insert(QStringLiteral("latestRelease"), releaseToJsonObject(result.latestRelease));
        object.insert(QStringLiteral("currentRelease"), releaseToJsonObject(result.currentRelease));

        if (!result.errorMessage.isEmpty())
            object.insert(QStringLiteral("error"), result.errorMessage);

        const QJsonDocument document(object);
        out << QString::fromUtf8(document.toJson(QJsonDocument::Compact)) << Qt::endl;
    }

    void printUpdateCheckErrorJson(
        QTextStream& out,
        const GitHubUpdateCheckResult& result)
    {
        QJsonObject object;

        object.insert(QStringLiteral("ok"), false);
        object.insert(QStringLiteral("error"), result.errorMessage);
        object.insert(QStringLiteral("httpStatus"), result.httpStatus);
        object.insert(QStringLiteral("apiUrl"), result.apiUrl.toString());

        const QJsonDocument document(object);
        out << QString::fromUtf8(document.toJson(QJsonDocument::Compact)) << Qt::endl;
    }

    int runUpdateCheckCommand(const QStringList& args)
    {
        QTextStream out(stdout);
        QTextStream err(stderr);

        bool includePrereleases = false;
        bool json = false;
        bool quiet = false;

        for (int i = 0; i < args.size(); ++i) {
            const QString arg = args.at(i);

            if (arg == "-h" || arg == "--help") {
                printUpdateCheckUsage(out);
                return 0;
            }

            if (arg == "--prerelease") {
                includePrereleases = true;
                continue;
            }

            if (arg == "--json") {
                json = true;
                continue;
            }

            if (arg == "--quiet") {
                quiet = true;
                continue;
            }

            err << "Unknown update-check option: " << arg << Qt::endl;
            printUpdateCheckUsage(err);
            return 2;
        }

        if (json && quiet) {
            err << "--json and --quiet cannot be used together." << Qt::endl;
            printUpdateCheckUsage(err);
            return 2;
        }

        GitHubUpdateCheckOptions options;
        options.owner = QStringLiteral("fatelarico");
        options.repo = QStringLiteral("PDFutils");
        options.currentVersion = QCoreApplication::applicationVersion();
        options.fallbackCurrentReleaseDate =
            QDateTime::fromString(QStringLiteral(APP_RELEASE_DATE), Qt::ISODate);
        options.includePrereleases = includePrereleases;
        options.userAgent = QStringLiteral("PDFutils-cli/%1").arg(APP_VERSION);

        const GitHubUpdateCheckResult result = checkGitHubForUpdates(options);

        if (!result.ok) {
            if (json) {
                printUpdateCheckErrorJson(out, result);
            } else if (!quiet) {
                err << result.errorMessage << Qt::endl;
            }

            return 1;
        }

        if (quiet)
            return result.updateAvailable ? 10 : 0;

        if (json) {
            printUpdateCheckJson(out, result);
        } else {
            printUpdateCheckText(out, result);
        }

        return result.updateAvailable ? 10 : 0;
    }

    /*
    * All `run*Command` should be above this line
    */


} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QCoreApplication::setApplicationName("PDFutils-cli");
    QCoreApplication::setOrganizationName("FATelarico");
    QCoreApplication::setApplicationVersion(APP_VERSION);

    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList arguments = app.arguments();

    if (arguments.size() <= 1) {
        printGeneralUsage(out);
        return 0;
    }

    const QString command = arguments.at(1).trimmed().toLower();

    if (command == "-h" || command == "--help") {
        printGeneralUsage(out);
        return 0;
    }

    if (command == "merge") {
        return runMergeCommand(arguments.mid(2));
    }

    if (command == "split") {
        return runSplitCommand(arguments.mid(2));
    }

    if (command == "extract") {
        return runExtractCommand(arguments.mid(2));
    }

    if (command == "insert") {
        return runInsertCommand(arguments.mid(2));
    }

    if (command == "compress") {
        return runCompressCommand(arguments.mid(2));
    }

    if (command == "img2pdf") {
        return runImageToPdfCommand(arguments.mid(2));
    }

    if (command == "update-check") {
        return runUpdateCheckCommand(arguments.mid(2));
    }

    err << "Unknown command: " << command << Qt::endl;
    printGeneralUsage(err);
    return 1;
}
