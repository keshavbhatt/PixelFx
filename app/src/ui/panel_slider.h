#pragma once

#include <QSlider>

namespace pixelfx::ui {

/// A slider that takes the mouse wheel only once it has the focus (clicked or tabbed to), so
/// the wheel scrolls the panel under the pointer instead of changing every slider it passes.
class PanelSlider : public QSlider
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PanelSlider)

public:
    explicit PanelSlider(Qt::Orientation orientation, QWidget* parent = nullptr);
    ~PanelSlider() override;

protected:
    void wheelEvent(QWheelEvent* event) override;
};

} // namespace pixelfx::ui
