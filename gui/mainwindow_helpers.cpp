#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "../updates/github_release_parser.h"

#include <QDateTime>
// #include <QDebug>
// #include <QJsonArray>
// #include <QJsonObject>
// #include <QJsonValue>
#include <QSettings>

void MainWindow::loadReleaseChannelPreference()
{
    QSettings settings;

    WantsPrerel = settings.value(
                              "updates/wantsPrereleases",
                              false
                              ).toBool();

    updateReleaseChannelActionText();
}

void MainWindow::saveReleaseChannelPreference() const
{
    QSettings settings;

    settings.setValue(
        "updates/wantsPrereleases",
        WantsPrerel
        );
}

void MainWindow::updateReleaseChannelActionText()
{
    ui->link00c->setChecked(WantsPrerel);

    /*
        ui->link00c->setText(
            WantsPrerel ? "Unstable channel" : "Stable channel"
        );
    */
}


// Was here
// bool MainWindow::releaseTagMatchesCurrentVersion(

// Was here
// MainWindow::GitHubReleaseInfo MainWindow::latestReleaseFromJson(


// Was here
// MainWindow::GitHubReleaseInfo MainWindow::releaseForCurrentVersionFromJson(

