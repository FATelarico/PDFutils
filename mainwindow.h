#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    void on_link00_triggered();
    bool WantsPrerel = false;
    void loadReleaseChannelPreference();
    void saveReleaseChannelPreference() const;
    void updateReleaseChannelActionText();
    void updateChannelActionText();
    void on_link00c_triggered();
    QNetworkAccessManager *networkManager = nullptr;
    struct GitHubReleaseInfo {
        QString tag;
        bool prerelease = false;
        QDateTime publishedAt;
        QString htmlUrl;

        bool isValid() const
        {
            return !tag.isEmpty() && publishedAt.isValid();
        }
    };

    GitHubReleaseInfo latestReleaseFromJson(
        const QJsonArray &releases,
        bool includePrereleases
        );

    GitHubReleaseInfo releaseForCurrentVersionFromJson(
        const QJsonArray &releases,
        const QString &currentVersion,
        bool includePrereleases
        );

    bool releaseTagMatchesCurrentVersion(
        const QString &releaseTag,
        const QString &currentVersion
        ) const;
};
#endif // MAINWINDOW_H
