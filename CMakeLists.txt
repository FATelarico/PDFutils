cmake_minimum_required(VERSION 3.21)

project(PDFutils VERSION 0.4.1 LANGUAGES CXX)

set(PDFUTILS_PRERELEASE "beta")
set(PDFUTILS_DISPLAY_VERSION "${PROJECT_VERSION}-${PDFUTILS_PRERELEASE}")
set(PDFUTILS_DEBIAN_VERSION "${PROJECT_VERSION}~${PDFUTILS_PRERELEASE}")
set(PDFUTILS_RELEASE_DATE "2026-05-01T00:00:00Z")
set(PDFUTILS_GITHUB_OWNER "fatelarico")
set(PDFUTILS_GITHUB_REPO "PDFutils")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

set(PDFUTILS_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${PDFUTILS_GENERATED_INCLUDE_DIR}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/release_metadata.h.in"
    "${PDFUTILS_GENERATED_INCLUDE_DIR}/release_metadata.h"
    @ONLY
)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets LinguistTools Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets LinguistTools Network)

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

if(WIN32)
    find_package(ZLIB REQUIRED)
    find_package(JPEG REQUIRED)
endif()

set(TS_FILES
    # translations/PDFutils_en_GB.ts # Source lanuage
    translations/PDFutils_it_IT.ts
)

# -----------------------------------------------------------------------------
# Core target
#
# Shared backend.
# -----------------------------------------------------------------------------

set(CORE_SOURCES
    core/operation_result.h
    core/operation_result.cpp

    core/page_range.h
    core/page_range.cpp

    core/qpdf_utils.h
    core/qpdf_utils.cpp

    core/merge_service.h
    core/merge_service.cpp

    core/split_service.h
    core/split_service.cpp

    core/extract_service.h
    core/extract_service.cpp

    core/insert_service.h
    core/insert_service.cpp

    core/compress_service.h
    core/compress_service.cpp

    core/image_to_pdf_service.h
    core/image_to_pdf_service.cpp
)

add_library(pdfutils_core STATIC
    ${CORE_SOURCES}
)

target_link_libraries(pdfutils_core
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        ${QPDF_TARGET}
)

target_include_directories(pdfutils_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/core
)

# -----------------------------------------------------------------------------
# Updates target.
# -----------------------------------------------------------------------------

add_library(PDFutils_updates STATIC
    updates/github_release_info.h
    updates/github_release_parser.h
    updates/github_release_parser.cpp
    updates/github_update_checker.h
    updates/github_update_checker.cpp
)

target_include_directories(PDFutils_updates PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/updates
    ${PDFUTILS_GENERATED_INCLUDE_DIR}
)

target_link_libraries(PDFutils_updates PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)

# -----------------------------------------------------------------------------
# GUI target.
# -----------------------------------------------------------------------------

set(GUI_SOURCES
    gui/main_gui.cpp

    gui/mainwindow.cpp
    gui/mainwindow.h
    gui/mainwindow.ui
    gui/mainwindow_helpers.cpp

    gui/merge.cpp
    gui/merge.h
    gui/merge.ui

    gui/split.cpp
    gui/split.h
    gui/split.ui

    gui/extract.cpp
    gui/extract.h
    gui/extract.ui

    gui/insert.cpp
    gui/insert.h
    gui/insert.ui

    gui/compress.cpp
    gui/compress.h
    gui/compress.ui

    gui/convertimg.cpp
    gui/convertimg.h
    gui/convertimg.ui

    gui/debugPrintSelections.cpp
    gui/debugPrintSelections.h

    gui/helptexts.cpp
    gui/helptexts.h

    gui/language_manager.cpp
    gui/language_manager.h

    gui/langselector.ui
    gui/langselector.cpp
    gui/langselector.h

    media/media.qrc
)

# These helpers are still intentionally excluded while they are manually included
# by their owning GUI source files. When each module is refactored, move its logic
# into pdfutils_core and compile the corresponding .cpp normally.
# helptexts.cpp is still included manually by merge.cpp until it is given a header.
# split_helpers.cpp is now refactored into split_service.cpp.
# extract_helpers.cpp
# insert_helpers.cpp
# compress_helpers.cpp

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/media/pdfutils.rc")
    list(APPEND GUI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/media/pdfutils.rc")
endif()

if(APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/media/pdfutils.icns")
    set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/media/pdfutils.icns")
    set_source_files_properties(${APP_ICON_MACOS}
        PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND GUI_SOURCES ${APP_ICON_MACOS})
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(PDFutils-gui
        MANUAL_FINALIZATION
        ${GUI_SOURCES}
    )
else()
    if(ANDROID)
        add_library(PDFutils-gui SHARED
            ${GUI_SOURCES}
        )
    else()
        add_executable(PDFutils-gui
            ${GUI_SOURCES}

        )
    endif()
endif()

# -----------------------------------------------------------------------------
# Translation targets.
#
# Use explicit project-level target names for both Qt 5 and Qt 6:
#
#   cmake --build build --target PDFutils_lupdate
#   cmake --build build --target PDFutils_lrelease
#
# The Qt helper macros create different target names depending on Qt major
# version and Qt minor version, so call the LinguistTools executables directly.
# -----------------------------------------------------------------------------
set(PDFUTILS_TRANSLATION_SOURCES
    ${GUI_SOURCES}
    ${CORE_SOURCES}
)

set(QM_FILES)
set(PDFUTILS_QM_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/translations")
file(MAKE_DIRECTORY "${PDFUTILS_QM_OUTPUT_DIR}")

if(TARGET Qt${QT_VERSION_MAJOR}::lupdate)
    set(PDFUTILS_LUPDATE_EXECUTABLE Qt${QT_VERSION_MAJOR}::lupdate)
else()
    find_program(PDFUTILS_LUPDATE_EXECUTABLE
        NAMES lupdate-qt${QT_VERSION_MAJOR} lupdate
    )
endif()

if(TARGET Qt${QT_VERSION_MAJOR}::lrelease)
    set(PDFUTILS_LRELEASE_EXECUTABLE Qt${QT_VERSION_MAJOR}::lrelease)
else()
    find_program(PDFUTILS_LRELEASE_EXECUTABLE
        NAMES lrelease-qt${QT_VERSION_MAJOR} lrelease
    )
endif()

if(NOT PDFUTILS_LUPDATE_EXECUTABLE)
    message(FATAL_ERROR "Could not find Qt lupdate. Install the Qt LinguistTools package for the selected Qt version.")
endif()

if(NOT PDFUTILS_LRELEASE_EXECUTABLE)
    message(FATAL_ERROR "Could not find Qt lrelease. Install the Qt LinguistTools package for the selected Qt version.")
endif()

find_program(PDFUTILS_QMAKE_EXECUTABLE
    NAMES qmake-qt${QT_VERSION_MAJOR} qmake6 qmake
)

set(PDFUTILS_QT_TRANSLATIONS_DIR "")

if(PDFUTILS_QMAKE_EXECUTABLE)
    execute_process(
        COMMAND "${PDFUTILS_QMAKE_EXECUTABLE}" -query QT_INSTALL_TRANSLATIONS
        OUTPUT_VARIABLE PDFUTILS_QT_TRANSLATIONS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

set(PDFUTILS_QT_TRANSLATION_FILES)

if(PDFUTILS_QT_TRANSLATIONS_DIR)
    foreach(_ts_file IN LISTS TS_FILES)
        get_filename_component(_ts_name "${_ts_file}" NAME_WE)
        string(REGEX REPLACE "^PDFutils_" "" _locale_name "${_ts_name}")

        set(_qt_translation_candidates "qtbase_${_locale_name}.qm")

        if(_locale_name MATCHES "^([A-Za-z]+)[_-].*")
            list(APPEND _qt_translation_candidates "qtbase_${CMAKE_MATCH_1}.qm")
        endif()

        foreach(_qt_translation_candidate IN LISTS _qt_translation_candidates)
            if(EXISTS "${PDFUTILS_QT_TRANSLATIONS_DIR}/${_qt_translation_candidate}")
                list(APPEND PDFUTILS_QT_TRANSLATION_FILES
                    "${PDFUTILS_QT_TRANSLATIONS_DIR}/${_qt_translation_candidate}"
                )
                break()
            endif()
        endforeach()
    endforeach()

    list(REMOVE_DUPLICATES PDFUTILS_QT_TRANSLATION_FILES)
endif()

add_custom_target(PDFutils_lupdate
    COMMAND ${PDFUTILS_LUPDATE_EXECUTABLE}
            ${PDFUTILS_TRANSLATION_SOURCES}
            -ts ${TS_FILES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Updating Qt translation source files"
    VERBATIM
)

foreach(_ts_file IN LISTS TS_FILES)
    if(IS_ABSOLUTE "${_ts_file}")
        set(_ts_path "${_ts_file}")
    else()
        set(_ts_path "${CMAKE_CURRENT_SOURCE_DIR}/${_ts_file}")
    endif()

    get_filename_component(_ts_name "${_ts_file}" NAME_WE)
    set(_qm_file "${PDFUTILS_QM_OUTPUT_DIR}/${_ts_name}.qm")

    add_custom_command(
        OUTPUT "${_qm_file}"
        COMMAND ${PDFUTILS_LRELEASE_EXECUTABLE}
                "${_ts_path}"
                -qm "${_qm_file}"
        DEPENDS "${_ts_path}"
        COMMENT "Compiling ${_ts_file}"
        VERBATIM
    )

    list(APPEND QM_FILES "${_qm_file}")
endforeach()

add_custom_target(PDFutils_lrelease
    DEPENDS ${QM_FILES}
)

add_custom_target(update_translations)
add_dependencies(update_translations PDFutils_lupdate)

add_custom_target(release_translations)
add_dependencies(release_translations PDFutils_lrelease)

if(APPLE)
    set(PDFUTILS_TRANSLATIONS_INSTALL_DIR "PDFutils-gui.app/Contents/Resources/translations")
elseif(WIN32)
    set(PDFUTILS_TRANSLATIONS_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}/translations")
else()
    set(PDFUTILS_TRANSLATIONS_INSTALL_DIR "${CMAKE_INSTALL_DATAROOTDIR}/pdfutils/translations")
endif()

install(
    DIRECTORY "${PDFUTILS_QM_OUTPUT_DIR}/"
    DESTINATION "${PDFUTILS_TRANSLATIONS_INSTALL_DIR}"
    FILES_MATCHING PATTERN "*.qm"
)

if(PDFUTILS_QT_TRANSLATION_FILES)
    install(
        FILES ${PDFUTILS_QT_TRANSLATION_FILES}
        DESTINATION "${PDFUTILS_TRANSLATIONS_INSTALL_DIR}"
    )
endif()

target_include_directories(PDFutils-gui
    PRIVATE
        ${PDFUTILS_GENERATED_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/gui
        ${CMAKE_CURRENT_SOURCE_DIR}/core
)

target_link_libraries(PDFutils-gui
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        PDFutils_updates
        pdfutils_core
)

# A normal GUI build also compiles the current .ts files into .qm
add_dependencies(PDFutils-gui PDFutils_lrelease)

# -----------------------------------------------------------------------------
# CLI target.
#
# The CLI must not link Qt::Widgets. It should parse arguments, build options,
# call pdfutils_core, print the result, and exit with a meaningful status code.
# -----------------------------------------------------------------------------
add_executable(PDFutils-cli
    cli/main_cli.cpp
)


target_link_libraries(PDFutils-cli
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        pdfutils_core
        PDFutils_updates
)

target_include_directories(PDFutils-cli
    PRIVATE
        ${PDFUTILS_GENERATED_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/cli
        ${CMAKE_CURRENT_SOURCE_DIR}/core
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.PDFutils)
endif()

set_target_properties(PDFutils-gui PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_NAME "PDFutils-gui"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

set_target_properties(PDFutils-cli PROPERTIES
    WIN32_EXECUTABLE FALSE
)

install(TARGETS PDFutils-gui PDFutils-cli
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32)
    install(IMPORTED_RUNTIME_ARTIFACTS
        ${QPDF_TARGET}
        ZLIB::ZLIB
        JPEG::JPEG
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
# -----------------------------------------------------------------------------
# Add tests for the CLI
# -----------------------------------------------------------------------------

enable_testing()

add_test(
    NAME compress_pdfs
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/integration/test_compress.sh
                 $<TARGET_FILE:PDFutils-cli>
)

add_test(
    NAME extract_pdfs
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/integration/test_extract.sh
                 $<TARGET_FILE:PDFutils-cli>
)

add_test(
    NAME convert_to_pdf
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/integration/test_img2pdf.sh
                 $<TARGET_FILE:PDFutils-cli>
)

add_test(
    NAME insert_pdfs
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/integration/test_insert.sh
                 $<TARGET_FILE:PDFutils-cli>
)

add_test(
    NAME merge_pdfs
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/integration/test_merge.sh
                 $<TARGET_FILE:PDFutils-cli>
)

add_test(
    NAME split_pdfs
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/integration/test_split.sh
                 $<TARGET_FILE:PDFutils-cli>
)

set_tests_properties(
    compress_pdfs
    extract_pdfs
    convert_to_pdf
    insert_pdfs
    merge_pdfs
    split_pdfs
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# -----------------------------------------------------------------------------
# Generate a Qt deployment script only when the Qt/CMake package provides
# qt_generate_deploy_app_script. Some Qt versions and Qt5 builds do not.
# Optional: Linux desktop launcher and icon.
# -----------------------------------------------------------------------------

if(QT_VERSION_MAJOR EQUAL 6
   AND COMMAND qt_generate_deploy_app_script
   AND (WIN32 OR APPLE)
)
    qt_generate_deploy_app_script(
        TARGET PDFutils-gui
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )

    install(SCRIPT ${deploy_script})
else()
    message(STATUS "Qt runtime deployment script disabled for this platform")
endif()

if(COMMAND qt_finalize_executable)
    qt_finalize_executable(PDFutils-gui)
endif()

# -----------------------------------------------------------------------------
# Package settings.
# -----------------------------------------------------------------------------
set(CPACK_PACKAGE_VERSION "${PDFUTILS_DISPLAY_VERSION}")
set(CPACK_DEBIAN_PACKAGE_VERSION "${PDFUTILS_DEBIAN_VERSION}")
set(CPACK_PACKAGE_VENDOR "FATelarico")
set(CPACK_PACKAGE_CONTACT "contact.fatelarico@dewdata.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt GUI and CLI utilities for working with PDF files")
set(CPACK_PACKAGE_DESCRIPTION
"PDFutils provides a Qt graphical application and command-line tool for common
PDF operations, including merging, splitting, extracting, inserting, compressing
and converting images to PDF.

The package contains both the desktop GUI executable and the command-line
interface, together with translations and project licence files.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/FATelarico/PDFutils")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENCE-docs")

# Strip installed binaries when CPack creates binary packages.
set(CPACK_STRIP_FILES TRUE)

# Debian packages should carry a compressed changelog under /usr/share/doc/<pkg>.
set(PDFUTILS_DEBIAN_CHANGELOG "${CMAKE_CURRENT_BINARY_DIR}/changelog.Debian")
set(PDFUTILS_DEBIAN_CHANGELOG_GZ "${CMAKE_CURRENT_BINARY_DIR}/changelog.Debian.gz")

file(WRITE "${PDFUTILS_DEBIAN_CHANGELOG}"
"pdfutils (${PDFUTILS_DEBIAN_VERSION}-1) unstable; urgency=medium

  * Release ${PDFUTILS_DISPLAY_VERSION}.

 -- FATelarico <contact.fatelarico@dewdata.com>  Tue, 09 Jun 2026 17:54:00 +0000
")

find_program(GZIP_EXECUTABLE gzip REQUIRED)

add_custom_command(
    OUTPUT "${PDFUTILS_DEBIAN_CHANGELOG_GZ}"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${PDFUTILS_DEBIAN_CHANGELOG}"
            "${PDFUTILS_DEBIAN_CHANGELOG}.tmp"
    COMMAND "${GZIP_EXECUTABLE}" -9 -n -f
            "${PDFUTILS_DEBIAN_CHANGELOG}.tmp"
    COMMAND "${CMAKE_COMMAND}" -E rename
            "${PDFUTILS_DEBIAN_CHANGELOG}.tmp.gz"
            "${PDFUTILS_DEBIAN_CHANGELOG_GZ}"
    DEPENDS "${PDFUTILS_DEBIAN_CHANGELOG}"
    VERBATIM
)

add_custom_target(PDFutils_debian_changelog
    DEPENDS "${PDFUTILS_DEBIAN_CHANGELOG_GZ}"
)

if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENCE"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/pdfutils
        RENAME copyright
    )
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENCE-docs"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/pdfutils
        RENAME LICENCE-docs
    )

    install(FILES "${PDFUTILS_DEBIAN_CHANGELOG_GZ}"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/pdfutils
        RENAME changelog.Debian.gz
    )
else()
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENCE-docs"
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENCE"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/PDFutils
    )
endif()

# Configure the Linux desktop file for the GUI executable.
set(APP_DISPLAY_NAME "PDFutils")
set(APP_EXECUTABLE_NAME "PDFutils-gui")
set(APP_ICON_NAME "pdfutils")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/media/pdfutils.desktop.in")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/media/pdfutils.desktop.in"
        "${CMAKE_CURRENT_BINARY_DIR}/media/pdfutils.desktop"
        @ONLY
    )

    if(UNIX AND NOT APPLE)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/media/pdfutils.desktop"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
        )
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/media/pdfutils.png"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps"
            RENAME "${APP_ICON_NAME}.png"
        )
        add_dependencies(PDFutils-gui PDFutils_debian_changelog)
        add_dependencies(PDFutils-cli PDFutils_debian_changelog)
    endif()
endif()

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")

    set(CPACK_PACKAGE_NAME "PDFutils")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "PDFutils")

    set(CPACK_NSIS_DISPLAY_NAME "PDFutils")
    set(CPACK_NSIS_PACKAGE_NAME "PDFutils")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

    set(CPACK_PACKAGE_EXECUTABLES
        "PDFutils-gui" "PDFutils GUI"
        "PDFutils-cli" "PDFutils CLI"
    )

    set(CPACK_CREATE_DESKTOP_LINKS
        "PDFutils-gui"
    )
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGE_NAME "PDFutils")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB;RPM")

    set(CPACK_PACKAGE_NAME "pdfutils")
    set(CPACK_PACKAGE_FILE_NAME "pdfutils-${PDFUTILS_DISPLAY_VERSION}-linux-x86_64")

    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "FATelarico <contact.fatelarico@dewdata.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

    set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
endif()

include(CPack)
