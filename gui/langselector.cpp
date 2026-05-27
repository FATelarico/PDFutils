#include "langselector.h"
#include "ui_langselector.h"

LangSelector::LangSelector(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LangSelector)
{
    ui->setupUi(this);
}

LangSelector::~LangSelector()
{
    delete ui;
}
