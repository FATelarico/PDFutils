#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

class QApplication;

namespace ThemeManager {

enum class AppTheme : unsigned char {
    System,
    Light,
    Dark
};

AppTheme loadThemePreference();
void saveThemePreference(AppTheme theme);
bool applyTheme(QApplication& app, AppTheme theme);

} // namespace ThemeManager

#endif // THEME_MANAGER_H
