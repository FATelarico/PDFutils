#ifndef COMPRESS_H
#define COMPRESS_H

#include <QDialog>

namespace Ui {
class compress;
}

class compress : public QDialog
{
    Q_OBJECT

public:
    explicit compress(QWidget *parent = nullptr);
    ~compress();

private:
    Ui::compress *ui;
    QString m_ghostscriptProgram;
};


#endif // COMPRESS_H
