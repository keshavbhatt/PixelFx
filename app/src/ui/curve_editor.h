#pragma once

#include <QColor>
#include <QWidget>

#include <pfx/settings.h>

namespace pixelfx::ui {

/// A tone curve editor: the grid, the identity diagonal and the
/// curve drawn with the library's own sampler, so what is drawn is what renders. Click to add a
/// point, drag to move it, double-click to remove it; the end points move only up and down.
class CurveEditor : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CurveEditor)

public:
    explicit CurveEditor(QWidget* parent = nullptr);
    ~CurveEditor() override;

    /// Shows `curve` (identity when it has fewer than two points) without emitting.
    void setCurve(const pfx::Curve& curve);
    [[nodiscard]] const pfx::Curve& curve() const { return m_points; }
    /// Where point `index` is drawn, in widget coordinates.
    [[nodiscard]] QPointF pointPosition(std::size_t index) const { return toWidget(m_points.at(index)); }
    [[nodiscard]] bool isIdentity() const;
    void setColor(const QColor& color);

    [[nodiscard]] QSize sizeHint() const override { return {280, 180}; }

Q_SIGNALS:
    void curveChanged(const pfx::Curve& curve);
    /// A drag ended, or a point was added or removed: a history step.
    void editFinished();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] QRectF plot() const;
    [[nodiscard]] QPointF toWidget(const pfx::CurvePoint& p) const;
    [[nodiscard]] pfx::CurvePoint fromWidget(const QPointF& p) const;
    [[nodiscard]] int hit(const QPointF& position) const;
    void moveDragged(const QPointF& position);

    pfx::Curve m_points;
    QColor m_color;
    int m_colorToken = -1;
    int m_drag = -1;
};

} // namespace pixelfx::ui
