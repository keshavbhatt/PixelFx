#include "ui/panel_slider.h"

#include <QWheelEvent>

namespace pixelfx::ui {

PanelSlider::PanelSlider(Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

PanelSlider::~PanelSlider() = default;

void PanelSlider::wheelEvent(QWheelEvent* event)
{
    if (!hasFocus()) {
        event->ignore(); // goes on to the scroll area
        return;
    }
    QSlider::wheelEvent(event);
}

} // namespace pixelfx::ui
