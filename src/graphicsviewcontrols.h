#ifndef GRAPHICSVIEWCONTROLS_H
#define GRAPHICSVIEWCONTROLS_H

#include <QWidget>
#include "graphicsview.h"

namespace Ui {
class graphicsViewControls;
}

class graphicsViewControls : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString filter READ getFilter WRITE setFilter NOTIFY filterChanged)

public:
    explicit graphicsViewControls(QWidget *parent = nullptr,
                                  graphicsView *graphicsViewWidget = nullptr);
    ~graphicsViewControls();

signals:
    void filterChanged(QString filter);

public slots:
    QString getFilter();

    void uncheckFitToView();

private slots:
    void setFilter(QString filterStr = "");

    void on_flipHorizontly_toggled(bool checked);

    void on_flipVertically_toggled(bool checked);

    QString flipState();

    QString rotateState();

    void on_rotateClockwise_clicked();

    void on_rotateAntiClockwise_clicked();

    void on_fit_toggled(bool checked);

private:
    int rotationAngle = 0;
    QString filter;
    Ui::graphicsViewControls *ui;
    graphicsView *graphicsViewWidget = nullptr;
};

#endif // GRAPHICSVIEWCONTROLS_H
