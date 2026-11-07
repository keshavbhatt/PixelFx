#pragma once

#include "core/crop_geometry.h"
#include "ui/canvas.h"

#include <QObject>
#include <QPointF>

namespace pixelfx::ui {

/// Fix by four corners on the canvas: four handles on the straightened photo, the shape between
/// them with thirds lines, the rest of the photo dimmed, and a magnifier beside the corner being
/// dragged. A corner only goes where the shape stays one the library can square
/// (pfx::cornersUsable). Tab picks the next corner, the arrow keys move it a screen pixel
/// (Shift: ten).
class CornerTool : public QObject, public CanvasTool
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CornerTool)

public:
    explicit CornerTool(Canvas* canvas);
    ~CornerTool() override;

    void setCorners(const core::Corners& corners);
    [[nodiscard]] const core::Corners& corners() const { return m_corners; }
    /// Back to the photo's own corners.
    void reset();
    [[nodiscard]] int selected() const { return m_selected; }
    /// Shows corner `index` as being dragged, its magnifier on: for a screenshot of the drag.
    void showDragging(int index);

    void paint(QPainter& painter, const QRectF& photo) override;
    bool press(const QMouseEvent& event, const QRectF& photo) override;
    void move(const QMouseEvent& event, const QRectF& photo) override;
    void release(const QMouseEvent& event, const QRectF& photo) override;
    [[nodiscard]] bool takesKey(const QKeyEvent& event) const override;
    void key(const QKeyEvent& event, const QRectF& photo) override;

    /// "Top left corner", for the corner at `index`.
    [[nodiscard]] static QString cornerName(int index);

Q_SIGNALS:
    /// A drag ended or a key moved a corner: the picture can be made again.
    void cornersChanged(const pixelfx::core::Corners& corners);

private:
    [[nodiscard]] static QPointF onScreen(const pfx::Corner& corner, const QRectF& photo);
    /// Moves corner `index` to `fraction` when the shape stays usable.
    bool moveCorner(int index, QPointF fraction);
    void select(int index);
    void paintMagnifier(QPainter& painter, const QRectF& photo) const;

    Canvas* m_canvas;
    core::Corners m_corners = core::ownCorners();
    int m_selected = 0;
    int m_dragging = -1;
};

} // namespace pixelfx::ui
