#include "language_manager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>

LanguageManager& LanguageManager::instance()
{
    static LanguageManager manager;
    return manager;
}

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
{
}

QString LanguageManager::currentLocale() const
{
    return m_currentLocale;
}

QString LanguageManager::translationsDir() const
{
    const QString appDir = QCoreApplication::applicationDirPath();

    // Development tree: build/translations
    const QString devPath = QDir(appDir).absoluteFilePath("../translations");
    if (QDir(devPath).exists()) {
        return QDir(devPath).canonicalPath();
    }

    // Installed Linux layout, for example:
    // /usr/bin/PDFutils-gui
    // /usr/share/pdfutils/translations/PDFutils_it_IT.qm
    const QString installedPath = QDir(appDir).absoluteFilePath("../share/pdfutils/translations");
    if (QDir(installedPath).exists()) {
        return QDir(installedPath).canonicalPath();
    }

    return appDir;
}

bool LanguageManager::setLanguage(const QString& localeName)
{
    qApp->removeTranslator(&m_translator);
    m_translator = QTranslator();

    // Source language: British English.
    // No .qm file needed; Qt falls back to the original tr() strings.
    if (localeName == "en_GB" || localeName == "en" || localeName.isEmpty()) {
        m_currentLocale = "en_GB";
        emit languageChanged();
        return true;
    }

    const QString fileName = "PDFutils_" + localeName;

    if (!m_translator.load(fileName, translationsDir())) {
        m_currentLocale = "en_GB";
        emit languageChanged();
        return false;
    }

    qApp->installTranslator(&m_translator);
    m_currentLocale = localeName;
    emit languageChanged();

    return true;
}
