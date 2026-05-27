#include "merge_service.h"
#include "split_service.h"

#include <QCoreApplication>
#include <QTextStream>

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

namespace
{
void printGeneralUsage(QTextStream& out)
{
    out << "PDFutils-cli " << APP_VERSION << Qt::endl;
    out << "Usage:" << Qt::endl;
    out << "  PDFutils-cli merge [options] input1.pdf input2.pdf ..." << Qt::endl;
    out << "  PDFutils-cli split [options] input1.pdf input2.pdf ..." << Qt::endl;
    out << Qt::endl;
    out << "Commands:" << Qt::endl;
    out << "  merge      Merge PDF files" << Qt::endl;
    out << "  split      Split PDF files" << Qt::endl;
    out << "  extract    Not implemented yet" << Qt::endl;
    out << "  insert     Not implemented yet" << Qt::endl;
    out << "  compress   Not implemented yet" << Qt::endl;
    out << "  img2pdf    Not implemented yet" << Qt::endl;
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

int commandNotImplemented(const QString& command)
{
    QTextStream err(stderr);
    err << "The '" << command << "' command is recognised but not implemented yet." << Qt::endl;
    return 2;
}

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

    for (const QString& createdFile : createdFiles) {
        out << createdFile << Qt::endl;
    }

    return 0;
}
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

    if (command == "extract" ||
        command == "insert" ||
        command == "compress" ||
        command == "img2pdf") {
        return commandNotImplemented(command);
    }

    err << "Unknown command: " << command << Qt::endl;
    printGeneralUsage(err);
    return 1;
}
