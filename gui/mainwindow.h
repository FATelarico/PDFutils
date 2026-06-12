#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "theme_manager.h"

// #include <QDateTime>
// #include <QJsonArray>
#include <QString>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QAction;
class QActionGroup;
class QEvent;
class QMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT

protected:
    void changeEvent(QEvent* event) override;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    ThemeManager::AppTheme m_themePreference = ThemeManager::AppTheme::System;
    QMenu* m_themeMenu = nullptr;
    QActionGroup* m_themeActionGroup = nullptr;
    QAction* m_followSystemThemeAction = nullptr;
    QAction* m_lightThemeAction = nullptr;
    QAction* m_darkThemeAction = nullptr;

    void on_link00f_triggered();
    bool applyLanguage(const QString& languageCode);

    void on_link00_triggered();
    bool WantsPrerel = false;
    void loadReleaseChannelPreference();
    void saveReleaseChannelPreference() const;
    void updateReleaseChannelActionText();
    void setupThemeMenu();
    void setThemePreference(ThemeManager::AppTheme theme);
    void updateThemeMenuText();
    void updateThemeActionState();
    void updateVersionMenuText();
    void on_link00c_triggered();
};
#endif // MAINWINDOW_H
