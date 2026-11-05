#pragma once

#include <QWidget>

#include <array>
#include <pfx/settings.h>

namespace pixelfx::ui {

/// The region curve (Lightroom's parametric curve): four tone regions shaded behind the curve,
/// drawn with the library's own sampler, and three marks under it for where the regions meet.
/// Drag a mark, or Tab to it and use Left and Right (Shift for 10). The region amounts are
/// ordinary sliders beside it; this view only draws them and moves the splits.
class RegionCurveView : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RegionCurveView)

public:
    explicit RegionCurveView(QWidget* parent = nullptr);
    ~RegionCurveView() override;

    /// Shows the region curve and splits of `settings` without emitting.
    void setSettings(const pfx::Settings& settings);
    /// Where split `index` is (0 to 2), 5 to 95.
    [[nodiscard]] double split(int index) const;
    /// Where mark `index` is drawn, in widget coordinates.
    [[nodiscard]] QPointF markPosition(int index) const;
    [[nodiscard]] QSize sizeHint() const override { return {280, kPlotHeight + kMarkStrip}; }

    static constexpr int kPlotHeight = 160;
    static constexpr int kMarkStrip = 18;

Q_SIGNALS:
    /// A split moved, live while dragging; `finished` when the drag or key ends (a history step).
    void splitMoved(int index, double value, bool finished);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private:
    [[nodiscard]] QRectF plot() const;
    void moveSplit(int index, double value, bool finished);
    void refreshDescription();

    pfx::Settings m_settings;
    std::array<double, 3> m_splits = {25.0, 50.0, 75.0};
    int m_drag = -1;
    int m_focused = 0;
    bool m_keyed = false;
};

} // namespace pixelfx::ui
