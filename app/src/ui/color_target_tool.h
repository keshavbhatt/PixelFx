#pragma once

#include "ui/canvas.h"

#include <QObject>
#include <QPointF>
#include <QString>

#include <optional>

namespace pixelfx::ui {

/// Picking a colour on the photo for the Color mixer and the B&W mix (Lightroom's targeted
/// adjustment): hovering shows what is under the pointer, a press picks it, and a vertical drag
/// changes it, up raising and down lowering. The tool only reports where and how far; the panel
/// works out the bands and the settings. A line at the top says what a drag does.
class ColorTargetTool : public QObject, public CanvasTool
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ColorTargetTool)

public:
    explicit ColorTargetTool(QObject* parent = nullptr);
    ~ColorTargetTool() override;

    /// Beside the marker: the bands under the pointer ("Blues and aquas") and what a drag does.
    void setHint(const QString& title, const QString& line);
    /// The line at the top of the canvas ("Drag on a colour to change its luminance").
    void setBanner(const QString& text);
    /// Where the marker is, as a fraction of the photo; empty while the pointer is off the photo.
    [[nodiscard]] std::optional<QPointF> marker() const { return m_marker; }
    /// The pointer over `fraction` of the photo, as a mouse move does.
    void hoverAt(QPointF fraction);
    /// A press at `fraction` and a drag `steps` up (negative: down), released.
    void dragAt(QPointF fraction, double steps);

    void paint(QPainter& painter, const QRectF& photo) override;
    bool press(const QMouseEvent& event, const QRectF& photo) override;
    void move(const QMouseEvent& event, const QRectF& photo) override;
    void release(const QMouseEvent& event, const QRectF& photo) override;
    void leave() override;

Q_SIGNALS:
    void hovered(QPointF fraction);
    void pressed(QPointF fraction);
    /// Steps from the press: one per 2 px up (Shift: per 8 px), negative down. `finished` on release.
    void dragged(double steps, bool finished);

private:
    [[nodiscard]] double stepsTo(const QMouseEvent& event) const;

    std::optional<QPointF> m_marker;
    QString m_title;
    QString m_line;
    QString m_banner;
    bool m_dragging = false;
    double m_pressY = 0.0;
};

} // namespace pixelfx::ui
