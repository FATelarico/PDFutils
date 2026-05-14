#ifndef SPLIT_H
#define SPLIT_H

#include <QDialog>

namespace Ui {
class split;
}

class split : public QDialog
{
    Q_OBJECT

public:
    explicit split(QWidget *parent = nullptr);
    ~split();

private:
    Ui::split *ui;
};

#endif // SPLIT_H
