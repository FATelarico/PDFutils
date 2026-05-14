#ifndef MERGE_H
#define MERGE_H

#include <QDialog>

namespace Ui {
class merge;
}

class merge : public QDialog
{
    Q_OBJECT

public:
    explicit merge(QWidget *parent = nullptr);
    ~merge();

private:
    Ui::merge *ui;
};

#endif // MERGE_H
