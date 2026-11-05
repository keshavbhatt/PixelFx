#pragma once

#include <QImage>
#include <QWidget>

#include <utility>

class QPainter;

namespace pixelfx::ui {

/// One colour grading wheel: the angle is the hue, clockwise from red
/// at the top, and the distance from the centre is the amount (0 to 100). Drag to pick, arrow
/// keys to nudge (Left and Right the hue, Up and Down the amount, Shift for 10), double-click or
/// Backspace to reset. It holds no image logic; the library's `grading` operation does the work.
class ColorWheel : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ColorWheel)

public:
    /// `name` is what screen readers hear first, for example "Shadows colour".
    explicit ColorWheel(const QString& name, QWidget* parent = nullptr, int diameter = kDiameter);
    ~ColorWheel() override;

    /// What screen readers hear first, when one wheel edits several ranges.
    void setName(const QString& name);
    [[nodiscard]] double hue() const { return m_hue; }
    [[nodiscard]] double amount() const { return m_amount; }
    /// Shows `hue` (degrees) and `amount` without emitting.
    void setColor(double hue, double amount);
    /// Where the puck is drawn, in widget coordinates.
    [[nodiscard]] QPointF puckPosition() const;
    /// The hue and amount a point in widget coordinates stands for.
    [[nodiscard]] std::pair<double, double> colorAt(const QPointF& position) const;

    [[nodiscard]] QSize sizeHint() const override { return {m_diameter, m_diameter}; }
    [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }

    static constexpr int kDiameter = 104;

Q_SIGNALS:
    /// Every change, live while dragging.
    void moved(double hue, double amount);
    /// A change is complete (drag ended, a key, a reset): a history step.
    void released(double hue, double amount);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    [[nodiscard]] QRectF disc() const;
    void change(double hue, double amount, bool finished);
    void refreshDescription();

    QString m_name;
    int m_diameter;
    double m_hue = 0.0;
    double m_amount = 0.0;
    bool m_dragging = false;
    bool m_keyed = false; // changed by keys since the last released()
    QImage m_disc;
};

/// Draws a colour wheel in `disc`: the hue ring fading to grey in the middle, a rim (the accent
/// when `focused`), and the puck at `hue` and `amount` with a white and a dark edge so it shows
/// on any colour. `cache` keeps the rendered disc between paints. Shared by the grading wheel
/// and the small wheels that choose a grading range.
void paintColorWheel(QPainter& painter, const QRectF& disc, QImage& cache, double hue, double amount,
                     double puck, bool focused);

} // namespace pixelfx::ui
