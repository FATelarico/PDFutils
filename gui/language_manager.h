#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTranslator>

class LanguageManager : public QObject
{
    Q_OBJECT

public:
    static LanguageManager& instance();

    QString currentLocale() const;
    QString translationDirectory() const;
    QStringList translationSearchPaths() const;
    bool initializeFromUiLanguages(const QStringList& uiLanguages);
    bool setLanguage(const QString& localeName);

signals:
    void languageChanged();

private:
    explicit LanguageManager(QObject* parent = nullptr);

    QStringList qtTranslationSearchPaths() const;

    QTranslator* m_qtTranslator = nullptr;
    QTranslator* m_translator = nullptr;
    QString m_currentLocale = "en_GB";
};
