cmake_minimum_required(VERSION 3.16)

project(PDFutils VERSION 0.3.0 LANGUAGES CXX)
# Add pre-release tags tags
set(PDFUTILS_PRERELEASE "alpha")
set(PDFUTILS_DISPLAY_VERSION "${PROJECT_VERSION}-${PDFUTILS_PRERELEASE}")
set(PDFUTILS_DEBIAN_VERSION "${PROJECT_VERSION}~${PDFUTILS_PRERELEASE}")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Network)

find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(LIBQPDF QUIET IMPORTED_TARGET libqpdf)
endif()

if(LIBQPDF_FOUND)
    set(QPDF_TARGET PkgConfig::LIBQPDF)
else()
    find_package(qpdf CONFIG REQUIRED)
    set(QPDF_TARGET qpdf::libqpdf)
endif()

set(TS_FILES
    PDFutils_en_GB.ts
)

set(PROJECT_SOURCES
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

    ${TS_FILES}
)

#
# Do NOT add these helper .cpp files here if they are included manually
# inside other .cpp files with #include "...cpp".
#
# split_helpers.cpp
# extract_helpers.cpp
# insert_helpers.cpp
# compress_helpers.cpp
#
# If they are normal separately compiled source files, remove this comment
# and add them to PROJECT_SOURCES properly.
#

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(PDFutils
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(PDFutils SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(PDFutils
            ${PROJECT_SOURCES}
            mainwindow_helpers.cpp
            convertimg.h convertimg.ui
            convertimg.cpp
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(PDFutils
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        ${QPDF_TARGET}
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.PDFutils)
endif()

set_target_properties(PDFutils PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS PDFutils
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

target_compile_definitions(PDFutils PRIVATE
    APP_VERSION="${PDFUTILS_DISPLAY_VERSION}"
    APP_RELEASE_DATE="2026-05-01"
)

#
# Optional Linux desktop launcher.
# media/pdfutils.will be used to make the app to appear in launchers.
#
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/media/pdfutils.desktop")
    install(FILES media/pdfutils.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )
endif()

#
# Optional application icon.
# Stored as media/pdfutils.png.
#
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/media/pdfutils.png")
    install(FILES media/pdfutils.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
        RENAME pdfutils.png
    )
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_generate_deploy_app_script(
        TARGET PDFutils
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )

    install(SCRIPT ${deploy_script})
    qt_finalize_executable(PDFutils)
endif()

#
# Package settings.
# Uses:
#   Linux:   DEB
#   Windows: ZIP and NSIS installer
#   macOS:   DragNDrop DMG
#
set(CPACK_PACKAGE_VERSION "${PDFUTILS_DISPLAY_VERSION}")
# A hyphen separates the upstream version from the Debian package revision, so
# CPACK_DEBIAN_PACKAGE_VERSION may contain '-' only when CPACK_DEBIAN_PACKAGE_RELEASE is set.
set(CPACK_DEBIAN_PACKAGE_VERSION "${PDFUTILS_DEBIAN_VERSION}")
set(CPACK_PACKAGE_VENDOR "FATelarico")
set(CPACK_PACKAGE_CONTACT "contact.fatelarico@dewdata.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt GUI utility for working with PDF files")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/FATelarico/PDFutils")

# Adds a licence file
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENCE")

if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENCE"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/pdfutils
        RENAME copyright
    )
else()
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENCE"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/PDFutils
    )
endif()

# Configures the Linux desktop file
set(APP_DISPLAY_NAME "PDFutils")
set(APP_EXECUTABLE_NAME "PDFutils")
set(APP_ICON_NAME "pdfutils")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/media/pdfutils.desktop.in"
    "${CMAKE_CURRENT_BINARY_DIR}/media/pdfutils.desktop"
    @ONLY
)

if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/media/pdfutils.desktop"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
    )
endif()

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")

    set(CPACK_PACKAGE_NAME "PDFutils")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "PDFutils")

    # Optional but useful for NSIS installers
    set(CPACK_NSIS_DISPLAY_NAME "PDFutils")
    set(CPACK_NSIS_PACKAGE_NAME "PDFutils")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

    set(APP_ICON_RESOURCE_WINDOWS
        "${CMAKE_CURRENT_SOURCE_DIR}/media/pdfutils.rc"
    )

    list(APPEND PROJECT_SOURCES
        ${APP_ICON_RESOURCE_WINDOWS}
    )

    # Start Menu shortcut
    set(CPACK_PACKAGE_EXECUTABLES
        "PDFutils" "PDFutils"
    )

    # Desktop shortcut
    set(CPACK_CREATE_DESKTOP_LINKS
        "PDFutils"
    )

elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")

    set(CPACK_PACKAGE_NAME "PDFutils")

    set(APP_ICON_MACOS
        "${CMAKE_CURRENT_SOURCE_DIR}/media/pdfutils.icns"
    )

    set_source_files_properties(${APP_ICON_MACOS}
        PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
    )

    list(APPEND PROJECT_SOURCES
        ${APP_ICON_MACOS}
    )

elseif(UNIX)
    set(CPACK_GENERATOR "DEB")

    # Debian package names should be lowercase
    set(CPACK_PACKAGE_NAME "pdfutils")

    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "FATelarico <contact.fatelarico@dewdata.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)
