#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "merge.h"
#include "split.h"
#include "extract.h"
#include "insert.h"
#include "compress.h"

#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

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
}

MainWindow::~MainWindow()
{
    delete ui;
}
