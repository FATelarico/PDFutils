# PDFutils

PDFutils is a Qt-based utility suite for working with PDF files. The source code builds two front ends: 

- `PDFutils-gui`, the Qt Widgets desktop application;
- `PDFutils-cli`, a command-line utility for scripting, batch processing, and headless use.

Both front ends use the same `pdfutils_core` static library as a shared backend for the GUI and CLI.

## Features

PDFutils provides practical PDF manipulation tools:

- Merge PDF files
- Split PDF files
- Extract selected pages from PDF files
- Insert one PDF into one or more other PDFs
- Compress PDF files safely (with `qpdf`) or aggressively (using `Ghostscript`)
- Convert image files to PDF
- Optionally generate hyperlinked tables of contents for merged and converted PDFs
- Provide both a graphical interface and a command-line interface
- Check for newer GitHub release builds from both the GUI and CLI.
- Support packaging for Linux, Windows, and macOS

> [!NOTE]
> The exact behaviour of each operation depends on the current implementation and release version.
> `PDFutils` is a utility for PDF manipulation. PDF files can vary significantly in structure, encoding, compression, metadata, annotations, forms, embedded images, and producer-specific behaviour. Always verify important output files before relying on them in production, archival, or legal workflows.

## Downloading a Release Build

Pre-built executables are distributed through the project's [release page](https://github.com/FATelarico/PDFutils/releases/latest). Open the release page, choose the latest suitable release, and download the package that matches your operating system and CPU architecture.

At the moment, the only available pre-built packages are for Debian-based Linux systems on x86/x86_64 hardware. Other platforms, including Windows and macOS, are supported by the source code and CMake packaging configuration, but release executables for those systems are not currently published.

Users on unsupported systems should build PDFutils from source.

## Architecture

The current architecture separates the project into four parts:

* `pdfutils_core`: the common PDF operation backend used by both front ends. In the current CMake build, it is compiled as a static library (not depend on `gui/`).
* `PDFutils-gui`: the Qt Widgets graphical front end. It collects widget state, builds option structs, calls core services, and displays results (may depend on `core/`).
* `PDFutils-cli`: the command-line front end. It parses arguments, builds the same option structs, calls the same core services, prints results, and exits with meaningful status codes (not depend on `gui/`).
* `PDFutils_updates`: update-checking support for GitHub release metadata, used by both the GUI and CLI.

## Current Project Structure


The project currently uses the following nested source layout:

```
PDFutils/
├── cli
│   └── main_cli.cpp
├── CMakeLists.txt
├── core
│   ├── compress_service.cpp
│   ├── compress_service.h
│   ├── extract_service.cpp
│   ├── extract_service.h
│   ├── image_to_pdf_service.cpp
│   ├── image_to_pdf_service.h
│   ├── insert_service.cpp
│   ├── insert_service.h
│   ├── merge_service.cpp
│   ├── merge_service.h
│   ├── operation_result.cpp
│   ├── operation_result.h
│   ├── page_range.cpp
│   ├── page_range.h
│   ├── qpdf_utils.cpp
│   ├── qpdf_utils.h
│   ├── split_service.cpp
│   └── split_service.h
├── gui
│   ├── compress.cpp
│   ├── compress.h
│   ├── compress.ui
│   ├── convertimg.cpp
│   ├── convertimg.h
│   ├── convertimg.ui
│   ├── debugPrintSelections.cpp
│   ├── debugPrintSelections.h
│   ├── extract.cpp
│   ├── extract.h
│   ├── extract.ui
│   ├── helptexts.cpp
│   ├── helptexts.h
│   ├── insert.cpp
│   ├── insert.h
│   ├── insert.ui
│   ├── langselector.cpp
│   ├── langselector.h
│   ├── langselector.ui
│   ├── main_gui.cpp
│   ├── mainwindow.cpp
│   ├── mainwindow.h
│   ├── mainwindow_helpers.cpp
│   ├── mainwindow.ui
│   ├── merge.cpp
│   ├── merge.h
│   ├── merge.ui
│   ├── split.cpp
│   ├── split.h
│   └── split.ui
├── LICENSE
├── LICENCE
├── main.cpp
├── media
│   ├── add-document.svg
│   ├── adobe.png
│   ├── array-numbers.svg
│   ├── bookmark.svg
│   ├── checkmark.svg
│   ├── choices.svg
│   ├── clean.svg
│   ├── compress.svg
│   ├── custom-utils-gui.png
│   ├── document-add.svg
│   ├── document-subtract.svg
│   ├── extract.svg
│   ├── flow-data.svg
│   ├── gs.png
│   ├── help.svg
│   ├── image.svg
│   ├── insert-page.svg
│   ├── MacOS.qss
│   ├── media.qrc
│   ├── menu.svg
│   ├── merge.svg
│   ├── minimize.svg
│   ├── page-scroll.svg
│   ├── pdf.png
│   ├── pdf.svg
│   ├── pdfutils.desktop.in
│   ├── pdfutils.icns
│   ├── pdfutils.ico
│   ├── pdfutils.png
│   ├── pdfutils.rc
│   ├── performance.svg
│   ├── select-range.svg
│   ├── split.svg
│   ├── table-of-contents.svg
│   └── translate.svg
├── README.md
├── translations
│   ├── PDFutils_it_IT.ts
└── updates
     ├── github_release_info.h
     ├── github_release_parser.cpp
     ├── github_release_parser.h
     ├── github_update_checker.cpp
     └── github_update_checker.h
```

## Core Services

The shared backend currently exposes the following service modules:

* `merge_service`;
* `split_service`;
* `extract_service`;
* `insert_service`;
* `compress_service`;
* `image_to_pdf_service`.


Each service follows the same pattern:

1. An options struct describes the requested operation.
2. A service function executes the operation.
3. The service returns `OperationResult`.

Example pattern:

```cpp
struct MergeOptions
{
    QStringList inputPaths;
    QString outputPath;
    QString pageRangeSpec;
    bool generateHyperlinkedToc = false;
};

OperationResult mergePdfs(const MergeOptions& options);
```

## Command-Line Interface

With the exception of `update-check`, the supported commands (`merge`, `split`, `extract`, `insert`, `compress`, `img2pdf`) follow the  general form:

```bash
PDFutils-cli <command> [options] input-files...
```

### Merge

```bash
# Merge all pages from input1.pdf and input2.pdf into merged.pdf.
PDFutils-cli merge -o merged.pdf input1.pdf input2.pdf

# Merge selected pages only:
# - pages 1-3 from input1.pdf
# - all pages from input2.pdf
PDFutils-cli merge -o merged.pdf --pages "1-3;all" input1.pdf input2.pdf

# Merge both PDFs and prepend a hyperlinked table of contents.
PDFutils-cli merge -o merged.pdf --toc input1.pdf input2.pdf
```

### Split

```bash
# Split input.pdf into one-page PDFs.
# Output files are created in /tmp using "_split" as the filename suffix.
PDFutils-cli split -o /tmp/_split.pdf input.pdf

# Explicitly split input.pdf into one-page PDFs.
# This is the same as the default split mode.
PDFutils-cli split -o /tmp/_split.pdf --single-pages input.pdf

# Split input.pdf into two-page chunks after even pages:
# 1-2, 3-4, 5-6, etc.
PDFutils-cli split -o /tmp/_split.pdf --two-pages-even input.pdf

# Split input.pdf after odd pages:
# 1, 2-3, 4-5, 6-7, etc.
PDFutils-cli split -o /tmp/_split.pdf --two-pages-odd input.pdf

# Split input.pdf after pages 3, 7, and 12.
# Output ranges are 1-3, 4-7, 8-12, and 13-end.
PDFutils-cli split -o /tmp/_split.pdf --points "3,7,12" input.pdf

# Split two PDFs with document-specific split points:
# - first.pdf after pages 3 and 7
# - second.pdf after pages 2 and 8
PDFutils-cli split -o /tmp/_split.pdf --points "3,7;2,8" first.pdf second.pdf

# Split input.pdf into chunks of 10 pages each:
# 1-10, 11-20, 21-30, etc.
PDFutils-cli split -o /tmp/_split.pdf --every 10 input.pdf

# Split input.pdf at bookmarks of level 1.
# Each matching bookmark starts a new output part.
PDFutils-cli split -o /tmp/_split.pdf --bookmarks 1 input.pdf
```

### Extract

```bash
# Extract pages 1-3 and page 8 from input.pdf.
# Because no mode is specified, the default is one output PDF per selected range.
PDFutils-cli extract -o /tmp/_extract.pdf --pages "1-3,8" input.pdf

# Extract document-specific ranges:
# - pages 1-3 from first.pdf
# - pages 5-7 from second.pdf
# Default mode creates one output PDF per selected range.
PDFutils-cli extract -o /tmp/_extract.pdf --pages "1-3;5-7" first.pdf second.pdf

# Extract pages 1-3 from each input PDF.
# --per-file creates one output PDF per input document.
PDFutils-cli extract -o /tmp/_extract.pdf --per-file --pages "1-3" first.pdf second.pdf

# Extract pages into one combined output PDF:
# - pages 1-3 from first.pdf
# - pages 5-7 from second.pdf
PDFutils-cli extract -o /tmp/extracted.pdf --all --pages "1-3;5-7" first.pdf second.pdf
```

### Insert

```bash
# Insert all pages from insert.pdf after every page of input.pdf.
# This is the default insertion mode.
PDFutils-cli insert -i insert.pdf -o output.pdf input.pdf

# Insert all pages from insert.pdf after every even page of input.pdf:
# after page 2, page 4, page 6, etc.
PDFutils-cli insert -i insert.pdf -o output.pdf --after-even input.pdf

# Insert all pages from insert.pdf after pages 3 and 7 of input.pdf.
PDFutils-cli insert -i insert.pdf -o output.pdf --after-pages "3,7" input.pdf

# Insert all pages from insert.pdf at document-specific positions:
# - after pages 3 and 7 in first.pdf
# - after pages 2 and 8 in second.pdf
PDFutils-cli insert -i insert.pdf -o output.pdf --after-pages "3,7;2,8" first.pdf second.pdf

# Insert all pages from insert.pdf every 10 pages in input.pdf:
# after page 10, page 20, page 30, etc.
PDFutils-cli insert -i insert.pdf -o output.pdf --every 10 input.pdf

# Insert all pages from insert.pdf before level-1 bookmark targets in input.pdf.
# Internally, a bookmark pointing to page N becomes insertion after page N-1.
PDFutils-cli insert -i insert.pdf -o output.pdf --bookmarks 1 input.pdf
```

### Compress

```bash
# Compress input.pdf using the default qpdf backend.
# This applies safe structural optimisation and writes compressed.pdf.
PDFutils-cli compress -o compressed.pdf input.pdf

# Compress input.pdf with qpdf image optimisation enabled and metadata removal requested.
# This may reduce file size further than structural optimisation alone.
PDFutils-cli compress -o compressed.pdf --optimize-images --remove-metadata input.pdf

# Compress input.pdf using Ghostscript with the ebook preset.
# This can reduce size more aggressively, but may alter PDF structure, metadata, forms, annotations, or colour handling.
PDFutils-cli compress -o compressed.pdf --ghostscript --gs-preset ebook input.pdf

# Compress input.pdf using a specific Ghostscript executable and custom image downsampling.
# Colour and greyscale images are downsampled to 150 dpi.
PDFutils-cli compress -o compressed.pdf --gs-program /usr/bin/gs --gs-custom --color-dpi 50 --gray-dpi 50 input.pdf
```
### Image to PDF

```bash
# Convert image1.png and image2.jpg into a single PDF.
# Each image becomes one PDF page.
PDFutils-cli img2pdf -o images.pdf image1.png image2.jpg

# Convert images into a PDF and add a hyperlinked table of contents.
# The table of contents is inserted at the beginning of the PDF.
PDFutils-cli img2pdf -o images.pdf --toc image1.png image2.jpg

# Convert images into a PDF using fill mode.
# Images fill the page area and may be cropped; margins are set to 0 mm.
PDFutils-cli img2pdf -o images.pdf --fill --margin-mm 0 image1.png image2.jpg

# Convert images into a PDF by stretching each image to the page area.
# The PDF render resolution is set to 150 dpi.
PDFutils-cli img2pdf -o images.pdf --stretch --dpi 150 image1.png image2.jpg
```


### Checking for Updates


PDFutils can check GitHub release metadata to determine whether a newer release build is available.

Update checking is implemented in the shared `PDFutils_updates` component and is available to both front ends:

- the GUI can check for updates from the desktop application;
- the CLI can check for updates from scripts, terminals, or headless environments.

Update checking requires network access and uses Qt Network.

```bash
# Check for newer stable releases.
PDFutils-cli update-check

# Include GitHub pre-releases in the check.
PDFutils-cli update-check --prerelease

# Print machine-readable JSON, useful for automation.
PDFutils-cli update-check --json

# Include pre-releases and print machine-readable JSON.
PDFutils-cli update-check --prerelease --json

# Print nothing and use only the exit statu, which is useful in shell
# scripts because the command communicates the result as an exit code.
PDFutils-cli update-check --quiet
```

## Stack

PDFutils is built with:

- C++17
- CMake 3.16 or newer
- `Qt 5` and `Qt 6` with the following components:
    * Core
    * Gui
    * Widgets
    * Network (used by the update-checking component)
    * Linguist Tools
- qpdf/libqpdf
- CPack for packaging

### Dependencies

To build PDFutils from source, install:

- A C++17-compatible compiler
- CMake 3.16 or newer
- Qt 5 or Qt 6 with Core, Gui, Widgets, Network, and LinguistTools components
- qpdf/libqpdf
- pkg-config, optional but supported
- Ghostscript, optional for Ghostscript compression

On systems where `pkg-config` can find `libqpdf`, the build uses the imported pkg-config target. Otherwise, CMake falls back to finding qpdf through its CMake package configuration.

### Optional Runtime Dependency: Ghostscript

Ghostscript is optional and is used only for Ghostscript-based PDF compression.

qpdf-based compression works through libqpdf. Ghostscript compression requires a Ghostscript executable:

- Linux/macOS: usually `gs`
- Windows: usually `gswin64c.exe` or `gswin32c.exe`

The GUI allows the user to select a Ghostscript executable. The CLI accepts `--gs-program`.

## Building & Running from Source

### Build from Source

Clone the repository:

```bash
git clone https://github.com/FATelarico/PDFutils.git
cd PDFutils
```

Configure the project:

```bash
cmake -S . -B build
```

Build both GUI and CLI:

```bash
cmake --build build
```

For a release build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On single-configuration generators, such as Unix Makefiles or Ninja, `CMAKE_BUILD_TYPE=Release` controls the build type.

On multi-configuration generators, such as Visual Studio, use `cmake --build build --config Release`

### Running from Source

After building, run the GUI:

```bash
./build/PDFutils-gui
```

Run the CLI:

```bash
./build/PDFutils-cli --help
```

On Windows, the executables are:

```powershell
PDFutils-gui.exe
PDFutils-cli.exe
```

On macOS, `PDFutils-gui` is configured as an application bundle.

### Installing from Source 

Install with:

```bash
cmake --install build
```

For multi-configuration generators:

```bash
cmake --install build --config Release
```

The install process installs both:

```text
PDFutils-gui
PDFutils-cli
```

On Linux, the build also installs:

- A desktop launcher, if available
- An application icon, if available
- Licence documentation

## Packaging

PDFutils uses CPack for release packaging.

Supported package generators:

| Platform | Package format |
| -------- | -------------- |
| Linux    | DEB            |
| Windows  | ZIP and NSIS   |
| macOS    | DragNDrop DMG  |

Create packages after building:

```bash
cd build
cpack
```

For a release configuration on multi-configuration generators:

```bash
cpack -C Release
```

### Linux Packaging

On Linux, PDFutils is packaged as a Debian package named `pdfutils` that installs both the command-line utility `PDFutils-cli` and the `PDFutils-gui` graphical front end.


The Debian package metadata includes:

- Section: `utils`
- Priority: `optional`
- Shared library dependency detection enabled through CPack

A desktop launcher is installed into the standard applications directory and generated from:

```text
media/pdfutils.desktop.in
```

Note that the launcher targets the GUI executable, while the CLI executable is installed as a normal command-line program.

### Windows Packaging

On Windows, PDFutils can be packaged as:

- A ZIP archive
- An NSIS installer

The Windows installer configuration includes:

- Application name: `PDFutils`
- Install directory: `PDFutils`
- GUI and CLI executables
- Start Menu shortcut
- Desktop shortcut for the GUI
- Uninstall-before-install support

### macOS Packaging

On macOS, PDFutils is configured as an application bundle and packaged using the CPack DragNDrop generator.

The macOS package is intended to produce a DMG-style distribution suitable for drag-and-drop installation.

## Translation Support

The project includes Qt translation support through Qt Linguist Tools.

English is the source language. The currently maintained translation source file is:

* `translations/PDFutils_it_IT.ts`

Translation source files are updated with the project translation target, compiled to `.qm` files during the build, and installed with the application package.

The GUI includes language-selection support through the language selector.

## Licence

PDFutils uses split licensing:

- Source code, build files, packaging scripts, and translation source files are licensed under the GNU Affero General Public License v3.0 or later. See `LICENCE-docs`.
- Documentation, including `README.md`, is licensed under Creative Commons Attribution-ShareAlike 4.0 International. See `LICENCE`.

Unless a file says otherwise, treat prose documentation as CC-BY-SA-4.0 and software source as AGPL-3.0-or-later.

Third-party names, logos, and trademarks remain subject to their respective owners' rights and are not relicensed by this notice.

The packaging configuration installs the software licence with the application package and installs the documentation licence alongside it.
