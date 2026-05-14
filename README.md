# PDFutils

PDFutils is a Qt-based desktop application for working with PDF files.

The application provides a graphical interface for common PDF operations, including merging, splitting, extracting, inserting, and compressing PDF content. It is written in C++17, built with CMake, and uses Qt Widgets for the user interface together with qpdf/libqpdf for PDF processing.

## Features

PDFutils is designed as a lightweight desktop utility for practical PDF manipulation tasks.

Current application modules include:

- Merge PDF files
- Split PDF files
- Extract pages or content from PDF files
- Insert PDF content
- Compress PDF files
- Display help text and user guidance inside the application
- Provide a Qt-based graphical user interface
- Support application icons and desktop launchers where available
- Support packaging for Linux, Windows, and macOS

The exact behaviour of each operation depends on the current implementation and release version.

## Technology Stack

PDFutils is built with:

- C++17
- CMake 3.16 or newer
- Qt Widgets
- Qt Linguist Tools
- qpdf/libqpdf
- CPack for packaging

The build system supports both Qt 5 and Qt 6.

## Supported Platforms

The CMake configuration includes packaging support for:

- Linux, using Debian packages
- Windows, using ZIP and NSIS installers
- macOS, using DragNDrop DMG packages

The application is configured as a GUI program on Windows and as a macOS application bundle on macOS.

## Dependencies

To build PDFutils from source, install the following dependencies:

- A C++17-compatible compiler
- CMake 3.16 or newer
- Qt 5 or Qt 6 with Widgets and LinguistTools components
- qpdf/libqpdf
- pkg-config, optional but supported

On systems where `pkg-config` can find `libqpdf`, the build will use the imported pkg-config target. Otherwise, CMake falls back to finding qpdf through its CMake package configuration.

## Building from Source

Clone the repository:

```bash
git clone https://github.com/FATelarico/PDFutils.git
cd PDFutils
````

Create a build directory:

```bash
mkdir build
cd build
```

Configure the project:

```bash
cmake ..
```

Build the application:

```bash
cmake --build .
```

For a release build:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

On single-configuration generators, such as Unix Makefiles or Ninja, `CMAKE_BUILD_TYPE=Release` controls the build type.

On multi-configuration generators, such as Visual Studio, use:

```bash
cmake --build . --config Release
```

## Running the Application

After building, run the generated `PDFutils` executable from the build output directory.

On Linux:

```bash
./PDFutils
```

On Windows, run:

```powershell
PDFutils.exe
```

On macOS, open the generated application bundle if building as a bundle.

## Installation

If installation rules are enabled for the selected platform, install with:

```bash
cmake --install .
```

For multi-configuration generators:

```bash
cmake --install . --config Release
```

The install process places the application binary in the appropriate runtime directory.

On Linux, the build also installs:

* A desktop launcher, if available
* An application icon, if available
* Licence documentation

## Packaging

PDFutils uses CPack for release packaging.

Supported package generators are:

| Platform | Package format |
| -------- | -------------- |
| Linux    | DEB            |
| Windows  | ZIP and NSIS   |
| macOS    | DragNDrop DMG  |

To create packages after building:

```bash
cpack
```

For a release configuration on multi-configuration generators:

```bash
cpack -C Release
```

Generated packages will use the project version defined in CMake.

## Linux Packaging

On Linux, PDFutils is packaged as a Debian package named:

```text
pdfutils
```

The Debian package metadata includes:

* Section: `utils`
* Priority: `optional`
* Shared library dependency detection enabled through CPack

A desktop launcher is generated from:

```text
media/pdfutils.desktop.in
```

and installed into the standard applications directory.

## Windows Packaging

On Windows, PDFutils can be packaged as:

* A ZIP archive
* An NSIS installer

The Windows installer configuration includes:

* Application name: `PDFutils`
* Install directory: `PDFutils`
* Start Menu shortcut
* Desktop shortcut
* Uninstall-before-install support

## macOS Packaging

On macOS, PDFutils is configured as an application bundle and packaged using the CPack DragNDrop generator.

The macOS package is intended to produce a DMG-style distribution suitable for drag-and-drop installation.

## Translation Support

The project includes Qt translation support through Qt Linguist Tools.

The configured translation source file is:

```text
PDFutils_en_GB.ts
```

Translation files are processed during the Qt build configuration.

## Project Structure

The CMake configuration lists the main application sources as:

```text
main.cpp
mainwindow.cpp
mainwindow.h
mainwindow.ui
merge.cpp
merge.h
merge.ui
split.cpp
split.h
split.ui
extract.cpp
extract.h
extract.ui
insert.cpp
insert.h
insert.ui
compress.cpp
compress.h
compress.ui
debugPrintSelections.cpp
debugPrintSelections.h
merge_helpers.cpp
helptexts.cpp
media.qrc
PDFutils_en_GB.ts
```

The project also references helper implementation files for split, extract, insert, and compress operations. These are intentionally not added directly to the main source list if they are included manually inside other source files.

## Licence

PDFutils is distributed under the licence included in the `LICENCE` file.

The packaging configuration installs this licence file with the application package.

## Project Metadata

* Project name: `PDFutils`
* Version: `0.1`
* Vendor: `FATelarico`
* Homepage: `https://github.com/FATelarico/PDFutils`

## Notes for Developers

The application target is named:

```text
PDFutils
```

The build system enables the following Qt CMake features:

```cmake
CMAKE_AUTOUIC
CMAKE_AUTOMOC
CMAKE_AUTORCC
```

This means Qt UI files, meta-object code, and resource files are handled automatically by CMake.

The application links privately against:

```text
Qt::Widgets
qpdf/libqpdf
```

When modifying source files, ensure that Qt UI files, headers, resource files, and translation files remain correctly listed in the CMake source configuration.

## Disclaimer

PDFutils is a desktop utility for PDF manipulation. PDF files can vary significantly in structure, encoding, compression, metadata, and producer-specific behaviour. Always verify important output files before relying on them in production or archival workflows.
