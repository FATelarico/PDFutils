#include "theme_manager.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QPalette>
#include <QSettings>

namespace ThemeManager {
namespace {

const char kThemeSettingKey[] = "appearance/theme";

QString themeSettingValue(AppTheme theme)
{
    switch (theme) {
    case AppTheme::Light:
        return QStringLiteral("light");
    case AppTheme::Dark:
        return QStringLiteral("dark");
    case AppTheme::System:
    default:
        return QStringLiteral("system");
    }
}

AppTheme themeFromSettingValue(const QString& value)
{
    if (value == QStringLiteral("light"))
        return AppTheme::Light;

    if (value == QStringLiteral("dark"))
        return AppTheme::Dark;

    return AppTheme::System;
}

AppTheme resolveEffectiveTheme(const QApplication& app, AppTheme theme)
{
    if (theme != AppTheme::System)
        return theme;

    const auto windowColor = app.palette().color(QPalette::Window);
    return windowColor.lightness() < 128 ? AppTheme::Dark : AppTheme::Light;
}

QString styleSheetResourcePath(AppTheme theme)
{
    switch (theme) {
    case AppTheme::Dark:
        return QStringLiteral(":/qt_material/dark_theme.qss");
    case AppTheme::Light:
    case AppTheme::System:
    default:
        return QStringLiteral(":/qt_material/light_theme.qss");
    }
}

void rewriteThemeUrls(QString& styleSheet)
{
    styleSheet.replace(
        QStringLiteral("icon_light:/active/"),
        QStringLiteral(":/theme_light/active/qt_material/theme_light/active/")
    );
    styleSheet.replace(
        QStringLiteral("icon_light:/disabled/"),
        QStringLiteral(":/theme_light/disabled/qt_material/theme_light/disabled/")
    );
    styleSheet.replace(
        QStringLiteral("icon_light:/primary/"),
        QStringLiteral(":/theme_light/primary/qt_material/theme_light/primary/")
    );
    styleSheet.replace(
        QStringLiteral("icon_dark:/active/"),
        QStringLiteral(":/theme_dark/active/qt_material/theme_dark/active/")
    );
    styleSheet.replace(
        QStringLiteral("icon_dark:/disabled/"),
        QStringLiteral(":/theme_dark/disabled/qt_material/theme_dark/disabled/")
    );
    styleSheet.replace(
        QStringLiteral("icon_dark:/primary/"),
        QStringLiteral(":/theme_dark/primary/qt_material/theme_dark/primary/")
    );
}

} // namespace

AppTheme loadThemePreference()
{
    QSettings settings;
    return themeFromSettingValue(settings.value(QString::fromLatin1(kThemeSettingKey)).toString());
}

void saveThemePreference(AppTheme theme)
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(kThemeSettingKey), themeSettingValue(theme));
}

bool applyTheme(QApplication& app, AppTheme theme)
{
    const AppTheme effectiveTheme = resolveEffectiveTheme(app, theme);
    QFile styleSheetFile(styleSheetResourcePath(effectiveTheme));

    if (!styleSheetFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[theme] Could not load stylesheet:" << styleSheetFile.fileName();
        return false;
    }

    QString styleSheet = QString::fromUtf8(styleSheetFile.readAll());

    // Dynamically load icon patches for the active theme (light or dark)
    const QString IconPatchPath =
        (effectiveTheme == AppTheme::Dark)
            ? QStringLiteral(":/qt_material/generated_dark_icons.qss")
            : QStringLiteral(":/qt_material/generated_light_icons.qss");

    QFile IconPatchFile(IconPatchPath);
    if (IconPatchFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        styleSheet += QStringLiteral("\n\n");
        styleSheet += QString::fromUtf8(IconPatchFile.readAll());
    }

    // Dynamically load pixmap patches for the active theme (light or dark)
    const QString PixmapPatchPath =
        (effectiveTheme == AppTheme::Dark)
            ? QStringLiteral(":/qt_material/generated_dark_pixmaps.qss")
            : QStringLiteral(":/qt_material/generated_light_pixmaps.qss");

    QFile PixmapPatchFile(PixmapPatchPath);
    if (PixmapPatchFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        styleSheet += QStringLiteral("\n\n");
        styleSheet += QString::fromUtf8(PixmapPatchFile.readAll());
    }

    rewriteThemeUrls(styleSheet);
    app.setStyleSheet(styleSheet);
    return true;
}

} // namespace ThemeManager
