#pragma once

#include <QWidget>

namespace pixelfx::ui {

/// The straighten ruler under the photo in Crop mode: -45 to 45 degrees with
/// the current angle written in the middle and marked on the ticks. Drag sideways, or use the
/// arrow keys (a tenth of a degree; Shift, a whole one), to turn the photo.
class StraightenDial : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(StraightenDial)

public:
    explicit StraightenDial(QWidget* parent = nullptr);
    ~StraightenDial() override;

    /// Shows `degrees` without emitting.
    void setAngle(double degrees);
    [[nodiscard]] double angle() const { return m_angle; }
    [[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
    void angleChanged(double degrees);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void change(double degrees);
    double m_angle = 0.0;
    double m_pressAngle = 0.0;
    double m_pressX = 0.0;
};

} // namespace pixelfx::ui
