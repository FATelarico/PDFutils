#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// #include <QDateTime>
// #include <QJsonArray>
#include <QTranslator>
#include <QString>
#include <QMainWindow>
// #include <QNetworkAccessManager>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QEvent;
class QNetworkAccessManager;

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
    QTranslator appTranslator;
    QString currentLanguageCode = "en_GB";
    bool applyLanguage(const QString& languageCode);
    QString translationsDirectory() const;

    void on_link00_triggered();
    bool WantsPrerel = false;
    void loadReleaseChannelPreference();
    void saveReleaseChannelPreference() const;
    void updateReleaseChannelActionText();
    // void updateChannelActionText(); // Duplicate of the above function
    void on_link00c_triggered();
    QNetworkAccessManager *networkManager = nullptr;
    /*
     * struct GitHubReleaseInfo {
     *   QString tag;
     *   bool prerelease = false;
     *   QDateTime publishedAt;
     *   QString htmlUrl;
     *
     *   bool isValid() const
     *   {
     *       return !tag.isEmpty() && publishedAt.isValid();
     *   }
     * }
     * GitHubReleaseInfo latestReleaseFromJson(
     *   const QJsonArray &releases,
     *   bool includePrereleases
     *   );
     * GitHubReleaseInfo releaseForCurrentVersionFromJson(
     *   const QJsonArray &releases,
     *   const QString &currentVersion,
     *   bool includePrereleases
     *   );
     * bool releaseTagMatchesCurrentVersion(
     *   const QString &releaseTag,
     *   const QString &currentVersion
     * ) const;
     */
};
#endif // MAINWINDOW_H
