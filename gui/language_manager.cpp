#include "language_manager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QtGlobal>

namespace
{
void appendUniquePath(QStringList& paths, const QString& path)
{
    if (path.trimmed().isEmpty())
        return;

    const QString cleanedPath = QDir::cleanPath(path);

    if (!paths.contains(cleanedPath))
        paths.append(cleanedPath);
}

QString qtTranslationsPath()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    return QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
}
}

LanguageManager& LanguageManager::instance()
{
    static LanguageManager manager;
    return manager;
}

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
{
    m_qtTranslator = new QTranslator(this);
    m_translator = new QTranslator(this);
}

QString LanguageManager::currentLocale() const
{
    return m_currentLocale;
}

QStringList LanguageManager::translationSearchPaths() const
{
    QStringList paths;
    const QString appDir = QCoreApplication::applicationDirPath();

    appendUniquePath(paths, QDir(appDir).absoluteFilePath("translations"));
    appendUniquePath(paths, QDir(appDir).absoluteFilePath("../translations"));
    appendUniquePath(paths, QDir(appDir).absoluteFilePath("../Resources/translations"));
    appendUniquePath(paths, QDir(appDir).absoluteFilePath("../share/pdfutils/translations"));

    return paths;
}

QString LanguageManager::translationDirectory() const
{
    for (const QString& path : translationSearchPaths()) {
        const QDir directory(path);

        if (directory.exists())
            return directory.canonicalPath();
    }

    return QString();
}

QStringList LanguageManager::qtTranslationSearchPaths() const
{
    QStringList paths;

    appendUniquePath(paths, translationDirectory());
    appendUniquePath(paths, qtTranslationsPath());

    return paths;
}

bool LanguageManager::initializeFromUiLanguages(const QStringList& uiLanguages)
{
    for (const QString& uiLanguage : uiLanguages) {
        const QString localeName = QLocale(uiLanguage).name();

        if (localeName.isEmpty() || localeName == "C")
            continue;

        if (setLanguage(localeName))
            return true;
    }

    return setLanguage(QStringLiteral("en_GB"));
}

bool LanguageManager::setLanguage(const QString& localeName)
{
    qApp->removeTranslator(m_translator);
    qApp->removeTranslator(m_qtTranslator);

    delete m_translator;
    delete m_qtTranslator;

    m_translator = new QTranslator(this);
    m_qtTranslator = new QTranslator(this);

    const QString normalisedLocaleName = QLocale(localeName).name();
    const QLocale locale(normalisedLocaleName);

    // Source language: British English.
    // No .qm file needed; Qt falls back to the original tr() strings.
    if (normalisedLocaleName.isEmpty() || locale.language() == QLocale::English) {
        m_currentLocale = "en_GB";
        emit languageChanged();
        return true;
    }

    bool appTranslationLoaded = false;

    for (const QString& path : translationSearchPaths()) {
        if (m_translator->load(locale, QStringLiteral("PDFutils"), QStringLiteral("_"), path)) {
            appTranslationLoaded = true;
            break;
        }
    }

    if (!appTranslationLoaded) {
        m_currentLocale = "en_GB";
        emit languageChanged();
        return false;
    }

    for (const QString& path : qtTranslationSearchPaths()) {
        if (m_qtTranslator->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"), path))
            break;
    }

    if (!m_qtTranslator->isEmpty())
        qApp->installTranslator(m_qtTranslator);

    qApp->installTranslator(m_translator);
    m_currentLocale = normalisedLocaleName;
    emit languageChanged();

    return true;
}
