#include "mainwindow.h"
/*
 * #ifndef APP_RELEASE_DATE
 * #define APP_RELEASE_DATE "2026-05-01T00:00:00Z"
 * #endif
 * # ifnde*f APP_VERSION
 * #define APP_VERSION "0.0.0"
 * #endif
*/

#include <QApplication>
#include <QCoreApplication>
#include <QLocale>
#include <QStringList>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("PDFutils-gui");
    QCoreApplication::setOrganizationName("FATelarico");
    QCoreApplication::setApplicationVersion(APP_VERSION);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();

    for (const QString& locale : uiLanguages) {
        const QString baseName = "PDFutils_" + QLocale(locale).name();

        if (translator.load(":/i18n/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }

    MainWindow window;
    window.show();

    return QCoreApplication::exec();
}
