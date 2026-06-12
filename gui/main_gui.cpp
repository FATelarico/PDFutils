#include "language_manager.h"
#include "mainwindow.h"
#include "release_metadata.h"
#include "theme_manager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLocale>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("PDFutils-gui");
    QCoreApplication::setOrganizationName("FATelarico");
    QCoreApplication::setApplicationVersion(QStringLiteral(PDFUTILS_DISPLAY_VERSION));

    ThemeManager::applyTheme(app, ThemeManager::loadThemePreference());

    LanguageManager::instance().initializeFromUiLanguages(QLocale::system().uiLanguages());

    MainWindow window;
    window.show();

    return QCoreApplication::exec();
}
