#ifndef HSPACER_H
#define HSPACER_H

#include <QWidget>

namespace Ui {
class HSpacer;
}

class HSpacer : public QWidget
{
    Q_OBJECT

public:
    explicit HSpacer(QWidget *parent = nullptr);
    ~HSpacer();

private:
    Ui::HSpacer *ui;
};

#endif // HSPACER_H
