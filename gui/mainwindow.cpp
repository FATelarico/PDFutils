#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "language_manager.h"
#include "release_metadata.h"
#include "../updates/github_update_checker.h"
#include "merge.h"
#include "split.h"
#include "extract.h"
#include "insert.h"
#include "compress.h"
#include "convertimg.h"
#include "./ui_langselector.h"

#include <QDialog>
#include <QEvent>

#include <QDebug>

#include <QSettings>
#include <QDateTime>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>

namespace
{
QString releaseTypeText(const GitHubReleaseInfo& release)
{
    return release.prerelease
               ? QCoreApplication::translate("MainWindow", "pre-release")
               : QCoreApplication::translate("MainWindow", "full release");
}

QString isoDateOrUnknown(const QDateTime& dateTime)
{
    if (!dateTime.isValid())
        return QCoreApplication::translate("MainWindow", "unknown");

    return dateTime.toUTC().date().toString(Qt::ISODate);
}
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        updateVersionMenuText();
        updateReleaseChannelActionText();
    }

    QMainWindow::changeEvent(event);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    updateVersionMenuText();

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

    const bool ok00f = connect(ui->link00f, &QAction::triggered,
                               this, &MainWindow::on_link00f_triggered);
    qDebug() << "[startup] link00f connection:" << ok00f;

    connect(ui->link00d, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("https://ghostscript.com/releases/gsdnld.html"));
    });

    connect(ui->link00e, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("https://qpdf.readthedocs.io/en/stable/"));
    });
}

void MainWindow::updateVersionMenuText()
{
    const QString versionText =
        tr("Version %1").arg(QStringLiteral(PDFUTILS_DISPLAY_VERSION));

    ui->menuVerify_updates->setTitle(versionText);
    ui->label00->setText(versionText);
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

void MainWindow::on_link00_triggered()
{
    qDebug() << "[link00a] QAction triggered";

    GitHubUpdateCheckOptions options;
    options.owner = QStringLiteral(PDFUTILS_GITHUB_OWNER);
    options.repo = QStringLiteral(PDFUTILS_GITHUB_REPO);
    options.currentVersion = QCoreApplication::applicationVersion();
    options.fallbackCurrentReleaseDate =
        QDateTime::fromString(QStringLiteral(PDFUTILS_RELEASE_DATE), Qt::ISODate);
    options.includePrereleases = WantsPrerel;
    options.userAgent =
        QStringLiteral("PDFutils-gui/%1").arg(QStringLiteral(PDFUTILS_DISPLAY_VERSION));

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const GitHubUpdateCheckResult result = checkGitHubForUpdates(options);
    QApplication::restoreOverrideCursor();

    if (!result.ok) {
        QMessageBox::warning(
            this,
            tr("GitHub request failed"),
            result.errorMessage
        );
        return;
    }

    QString message =
        tr("Current version: %1\n"
           "Current release date: %2%3\n\n"
           "Latest release: %4\n"
           "Latest release type: %5\n"
           "Latest release date: %6")
            .arg(result.currentVersion,
                 isoDateOrUnknown(result.currentReleaseDate),
                 result.currentReleaseDateFromApi ? tr(" (API)") : QString(),
                 result.latestRelease.tag,
                 releaseTypeText(result.latestRelease),
                 isoDateOrUnknown(result.latestRelease.publishedAt));

    if (result.updateAvailable) {
        message += tr("\n\nAn update is available.");
    } else {
        message += tr("\n\nYou are using the latest available release.");
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("Release check"));
    box.setText(message);
    box.setIcon(QMessageBox::Information);

    QPushButton* updateButton = nullptr;

    if (result.updateAvailable)
        updateButton = box.addButton(tr("Update"), QMessageBox::AcceptRole);

    box.addButton(QMessageBox::Close);
    box.exec();

    if (!updateButton || box.clickedButton() != updateButton)
        return;

    const QString releaseUrl = result.latestReleaseUrl.toString();
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

bool MainWindow::applyLanguage(const QString& languageCode)
{
    if (!LanguageManager::instance().setLanguage(languageCode)) {
        QMessageBox::warning(
            this,
            tr("Language not available"),
            tr("Could not load the translation files for locale %1.\n\nSearched in:\n%2")
                .arg(QLocale(languageCode).name(),
                     LanguageManager::instance().translationSearchPaths().join("\n"))
        );
        return false;
    }

    return true;
}

void MainWindow::on_link00f_triggered()
{
    QDialog dialog(this);
    Ui::LangSelector languageUi;
    languageUi.setupUi(&dialog);

    if (LanguageManager::instance().currentLocale() == "it_IT") {
        languageUi.combo49->setCurrentIndex(1);
    } else {
        languageUi.combo49->setCurrentIndex(0);
    }

    connect(languageUi.btn50, &QPushButton::clicked,
            &dialog,
            [this, &dialog, &languageUi]() {
                const QString selectedLanguageCode =
                    languageUi.combo49->currentIndex() == 1
                        ? QStringLiteral("it_IT")
                        : QStringLiteral("en_GB");

                if (applyLanguage(selectedLanguageCode)) {
                    dialog.accept();
                }
            });

    dialog.exec();
}



MainWindow::~MainWindow()
{
    delete ui;
}
