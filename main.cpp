#include "mainwindow.h"

#ifndef APP_RELEASE_DATE
#define APP_RELEASE_DATE "2026-05-01T00:00:00Z"
#endif

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setApplicationName("PDFutils");
    QCoreApplication::setOrganizationName("FATelarico");
    QCoreApplication::setApplicationVersion(APP_VERSION);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "PDFutils_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
