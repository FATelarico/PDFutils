#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// #include <QDateTime>
// #include <QJsonArray>
#include <QString>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QEvent;

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

    void on_link00f_triggered();
    bool applyLanguage(const QString& languageCode);

    void on_link00_triggered();
    bool WantsPrerel = false;
    void loadReleaseChannelPreference();
    void saveReleaseChannelPreference() const;
    void updateReleaseChannelActionText();
    void updateVersionMenuText();
    void on_link00c_triggered();
};
#endif // MAINWINDOW_H
