#ifndef CORESLIDER_H
#define CORESLIDER_H

#include <QObject>
#include <QWidget>
#include <QSlider>
#include <QMouseEvent>
#include <QToolTip>
#include <QStyleOptionSlider>

class coreSlider : public QSlider
{
public:
    coreSlider(QWidget *parent =nullptr);

public slots:
    void reset();
    void setDefaultValue(int value);

protected slots:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *ev);
private slots:
    void mouse_event(QMouseEvent *ev);
private:
    int defaultValue;

};

#endif // CORESLIDER_H
