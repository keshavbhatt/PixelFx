#include "coreslider.h"
#include <QDebug>
#include <QCoreApplication>

coreSlider::coreSlider(QWidget *parent) :
    QSlider(parent)
{
    setMouseTracking(true);
}

void coreSlider::mousePressEvent(QMouseEvent *ev)
{
    if ( ev->button() == Qt::LeftButton )
    {
        ev->accept();
        setSliderDown(true);
        mouse_event(ev);
        update();
    }
    else
    {
        QSlider::mousePressEvent(ev);
    }
}

void coreSlider::mouseMoveEvent(QMouseEvent *ev)
{
    if ( ev->buttons() & Qt::LeftButton )
    {
        ev->accept();
        mouse_event(ev);
        update();
    }
    else
    {
        qreal pos = (geometry().width() > 5) ?
            static_cast<qreal>(ev->pos().x() - 2.5) / (geometry().width() - 5) : 0;
        pos = qMax(qMin(pos, 1.0), 0.0);
        int value = qRound(minimum() +
            pos * (maximum() - minimum()));

        if(value >= minimum() && value <= maximum())
            QToolTip::showText( QCursor::pos(), QString::number(value));

        QSlider::mouseMoveEvent(ev);
    }
}

void coreSlider::mouseReleaseEvent(QMouseEvent *ev)
{
    if ( ev->button() == Qt::LeftButton )
    {
        ev->accept();
        setSliderDown(false);
        update();
    }
    else
    {
        QSlider::mousePressEvent(ev);
    }
}

void coreSlider::mouse_event(QMouseEvent *ev)
{
    qreal pos = (geometry().width() > 5) ?
        static_cast<qreal>(ev->pos().x() - 2.5) / (geometry().width() - 5) : 0;
    pos = qMax(qMin(pos, 1.0), 0.0);
    setSliderPosition(qRound(minimum() +
        pos * (maximum() - minimum())));
}

void coreSlider::setDefaultValue(int value)
{
    defaultValue = value;
    setValue(value);
}

void coreSlider::reset()
{
    this->setValue(defaultValue);
}
