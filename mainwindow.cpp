#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "merge.h"
#include "split.h"
#include "extract.h"
#include "insert.h"
#include "compress.h"
#include "convertimg.h"

#include <QDebug>

// #include <QSettings>
// #include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
// #include <QJsonObject>
// #include <QJsonValue>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);



    loadReleaseChannelPreference();

    connect(ui->btn03_merge, &QPushButton::clicked, this, [this]() {
        // QMessageBox::information(this, "Clicked", "Button was clicked"); // Only opens a pop-up
        merge *mergeWindow = new merge(this);
        mergeWindow->show();
    });

    connect(ui->btn04_split, &QPushButton::clicked, this, [this]() {
        // QMessageBox::information(this, "Clicked", "Button was clicked"); // Only opens a pop-up
        split *splitWindow = new split(this);
        splitWindow->show();
    });

    connect(ui->btn05_extract, &QPushButton::clicked, this, [this]() {
        // QMessageBox::information(this, "Clicked", "Button was clicked"); // Only opens a pop-up
        extract *extractWindow = new extract(this);
        extractWindow->show();
    });

    connect(ui->btn06_insert, &QPushButton::clicked, this, [this]() {
        // QMessageBox::information(this, "Clicked", "Button was clicked"); // Only opens a pop-up
        insert *insertWindow = new insert(this);
        insertWindow->show();
    });

    connect(ui->btn07_compress, &QPushButton::clicked, this, [this]() {
        // QMessageBox::information(this, "Clicked", "Button was clicked"); // Only opens a pop-up
        compress *compressWindow = new compress(this);
        compressWindow->show();
    });

    connect(ui->btn09_img, &QPushButton::clicked, this, [this]() {
        // QMessageBox::information(this, "Clicked", "Button was clicked"); // Only opens a pop-up
        convertimg *convertimgWindow = new convertimg(this);
        convertimgWindow->show();
    });

    connect(ui->link00a, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/FATelarico/PDFutils"));
    });

    const bool ok00 = connect(ui->link00, &QAction::triggered,
                              this, &MainWindow::on_link00_triggered);
    qDebug() << "[startup] link00 connection:" << ok00;

    const bool ok00c = connect(ui->link00c, &QAction::triggered,
                               this, &MainWindow::on_link00c_triggered);
    qDebug() << "[startup] link00c connection:" << ok00c;

    connect(ui->link00d, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("https://ghostscript.com/releases/gsdnld.html"));
    });

    connect(ui->link00e, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("https://qpdf.readthedocs.io/en/stable/"));
    });
}


void MainWindow::on_link00c_triggered()
{
    WantsPrerel = !WantsPrerel;

    saveReleaseChannelPreference();
    updateReleaseChannelActionText();

    qDebug() << "[GitHub] Release channel preference saved:"
             << (WantsPrerel ? "unstable / prereleases allowed"
                             : "stable / prereleases ignored");
}

void MainWindow::on_link00_triggered(){
        qDebug() << "[link00a] QAction triggered";

        const QString owner = "fatelarico";
        const QString repo  = "PDFutils"; // "InView-Highlighter";

        if (!networkManager)
            networkManager = new QNetworkAccessManager(this);

        const QUrl apiUrl(
            QString("https://api.github.com/repos/%1/%2/releases?per_page=100")
                .arg(owner, repo)
            );

        QNetworkRequest request(apiUrl);
        request.setHeader(QNetworkRequest::UserAgentHeader, "PDFutils");
        request.setRawHeader("Accept", "application/vnd.github+json");
        request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

        QNetworkReply *reply = networkManager->get(request);

        connect(reply, &QNetworkReply::finished, this, [this, reply, owner, repo]() {
            const auto networkError = reply->error();
            const QString errorString = reply->errorString();

            const int httpStatus =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            const QByteArray data = reply->readAll();

            reply->deleteLater();

            qDebug() << "[GitHub] HTTP status:" << httpStatus;
            qDebug() << "[GitHub] Network error:" << networkError;
            qDebug() << "[GitHub] Error string:" << errorString;
            qDebug() << "[GitHub] Response size:" << data.size();

            if (networkError != QNetworkReply::NoError || httpStatus < 200 || httpStatus >= 300) {
                QMessageBox::warning(
                    this,
                    tr("GitHub request failed"),
                    tr("Could not retrieve GitHub releases.\n\nHTTP status: %1\n%2")
                        .arg(httpStatus)
                        .arg(errorString)
                    );
                return;
            }

            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

            if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
                QMessageBox::warning(
                    this,
                    tr("Invalid GitHub response"),
                    tr("The GitHub releases response could not be parsed.")
                    );
                return;
            }

            const QJsonArray releases = doc.array();

            const GitHubReleaseInfo latestRelease = latestReleaseFromJson(releases, WantsPrerel);

            if (!latestRelease.isValid()) {
                QMessageBox::information(
                    this,
                    tr("No release found"),
                    tr("No valid GitHub release was found for %1/%2.")
                        .arg(owner, repo)
                    );
                return;
            }

            const QString currentVersion = QCoreApplication::applicationVersion().isEmpty()
                                               ? tr("unknown")
                                               : QCoreApplication::applicationVersion();

            GitHubReleaseInfo currentRelease =
                releaseForCurrentVersionFromJson(releases, currentVersion, WantsPrerel);

            bool currentReleaseDateFromApi = currentRelease.isValid();

            QDateTime currentReleaseDate;

            if (currentReleaseDateFromApi) {
                currentReleaseDate = currentRelease.publishedAt;
            } else {
                currentReleaseDate =
                    QDateTime::fromString(QStringLiteral(APP_RELEASE_DATE), Qt::ISODate);

                qDebug() << "[GitHub] Current version release was not found in API response.";
                qDebug() << "[GitHub] Using hard-coded APP_RELEASE_DATE:"
                         << currentReleaseDate.toString(Qt::ISODate);
            }

            const QString latestReleaseType = latestRelease.prerelease
                                                  ? tr("pre-release")
                                                  : tr("full release");

            const QDate currentReleaseDay = currentReleaseDate.toUTC().date();
            const QDate latestReleaseDay = latestRelease.publishedAt.toUTC().date();

            const bool updateAvailable =
                currentReleaseDay.isValid() &&
                latestReleaseDay.isValid() &&
                latestReleaseDay > currentReleaseDay;

            QString message =
                tr("Current version: %1\n"
                   "Current release date: %2%3\n\n"
                   "Latest release: %4\n"
                   "Latest release type: %5\n"
                   "Latest release date: %6")
                    .arg(currentVersion,
                                       currentReleaseDay.isValid()
                                           ? currentReleaseDay.toString(Qt::ISODate)
                                           : tr("unknown"),
                                       currentReleaseDateFromApi
                                           ? tr("(API)")
                                           : tr(""),
                                       latestRelease.tag,
                                       latestReleaseType,
                                       latestReleaseDay.toString(Qt::ISODate));

            if (updateAvailable) {
                message += tr("\n\nAn update is available.");
            } else {
                message += tr("\n\nYou are using the latest available release.");
            }

            QMessageBox box(this);
            box.setWindowTitle(tr("Release check"));
            box.setText(message);
            box.setIcon(QMessageBox::Information);

            QPushButton *updateButton = nullptr;

            if (updateAvailable) {
                updateButton = box.addButton(tr("Update"), QMessageBox::AcceptRole);
            }

            box.addButton(QMessageBox::Close);

            box.exec();

            if (updateButton && box.clickedButton() == updateButton) {
                QString releaseUrl = latestRelease.htmlUrl;

                if (releaseUrl.isEmpty()) {
                    releaseUrl = QString("https://github.com/%1/%2/releases/tag/%3")
                    .arg(owner, repo, latestRelease.tag);
                }

                qDebug() << "[GitHub] Opening latest release page:" << releaseUrl;

                const bool opened = QDesktopServices::openUrl(QUrl(releaseUrl));

                if (!opened) {
                    QMessageBox::warning(
                        this,
                        tr("Browser error"),
                        tr("Could not open the latest release page:\n%1").arg(releaseUrl)
                        );
                }
            }
        });
    }


MainWindow::~MainWindow()
{
    delete ui;
}
