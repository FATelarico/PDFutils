#pragma once

#include <QObject>
#include <QTranslator>
#include <QString>

class LanguageManager : public QObject
{
    Q_OBJECT

public:
    static LanguageManager& instance();

    QString currentLocale() const;
    bool setLanguage(const QString& localeName);

signals:
    void languageChanged();

private:
    explicit LanguageManager(QObject* parent = nullptr);

    QString translationsDir() const;

    QTranslator m_translator;
    QString m_currentLocale = "en_GB";
};
