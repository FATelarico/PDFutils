#ifndef LANGSELECTOR_H
#define LANGSELECTOR_H

#include <QDialog>

namespace Ui {
class LangSelector;
}

class LangSelector : public QDialog
{
    Q_OBJECT

public:
    explicit LangSelector(QWidget *parent = nullptr);
    ~LangSelector();

private:
    Ui::LangSelector *ui;
};

#endif // LANGSELECTOR_H
