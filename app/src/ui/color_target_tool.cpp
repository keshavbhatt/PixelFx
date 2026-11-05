#include "ui/color_target_tool.h"

#include "ui/style.h"

#include <QMouseEvent>
#include <QPaintDevice>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace pixelfx::ui {

namespace {

constexpr double kMarker = 9.0;   // the ring's radius
constexpr double kHintGap = 18.0; // from the marker to the hint
constexpr double kPixelsPerStep = 2.0;
constexpr double kFinePixelsPerStep = 8.0;

QPointF fractionOf(const QMouseEvent& event, const QRectF& photo)
{
    const QPointF p = event.position();
    return {std::clamp((p.x() - photo.left()) / photo.width(), 0.0, 1.0),
            std::clamp((p.y() - photo.top()) / photo.height(), 0.0, 1.0)};
}

/// A rounded card on the elevated colour with a hairline, as menus and toasts are drawn.
void drawCard(QPainter& p, const QRectF& r)
{
    const Tokens& t = tokens();
    p.setPen(QPen(t.border, 1.0));
    p.setBrush(t.elevated);
    p.drawRoundedRect(r, 8, 8);
}

} // namespace

ColorTargetTool::ColorTargetTool(QObject* parent)
    : QObject(parent)
{}

ColorTargetTool::~ColorTargetTool() = default;

void ColorTargetTool::setHint(const QString& title, const QString& line)
{
    m_title = title;
    m_line = line;
}

void ColorTargetTool::setBanner(const QString& text)
{
    m_banner = text;
}

void ColorTargetTool::hoverAt(QPointF fraction)
{
    m_marker = fraction;
    Q_EMIT hovered(fraction);
}

void ColorTargetTool::dragAt(QPointF fraction, double steps)
{
    m_marker = fraction;
    Q_EMIT pressed(fraction);
    Q_EMIT dragged(steps, true);
}

double ColorTargetTool::stepsTo(const QMouseEvent& event) const
{
    const double perStep =
        event.modifiers().testFlag(Qt::ShiftModifier) ? kFinePixelsPerStep : kPixelsPerStep;
    return (m_pressY - event.position().y()) / perStep;
}

bool ColorTargetTool::press(const QMouseEvent& event, const QRectF& photo)
{
    if (!photo.contains(event.position())) {
        return false;
    }
    m_marker = fractionOf(event, photo);
    m_dragging = true;
    m_pressY = event.position().y();
    Q_EMIT pressed(*m_marker);
    return true;
}

void ColorTargetTool::move(const QMouseEvent& event, const QRectF& photo)
{
    if (m_dragging) {
        // The marker stays on the colour picked; only the distance counts.
        Q_EMIT dragged(stepsTo(event), false);
        return;
    }
    if (!photo.contains(event.position())) {
        m_marker.reset();
        return;
    }
    m_marker = fractionOf(event, photo);
    Q_EMIT hovered(*m_marker);
}

void ColorTargetTool::release(const QMouseEvent& event, const QRectF& /*photo*/)
{
    if (m_dragging) {
        m_dragging = false;
        Q_EMIT dragged(stepsTo(event), true);
    }
}

void ColorTargetTool::leave()
{
    if (!m_dragging) {
        m_marker.reset();
    }
}

void ColorTargetTool::paint(QPainter& p, const QRectF& photo)
{
    const Tokens& t = tokens();
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QFont small = p.font();
    small.setPixelSize(12);
    const QFontMetricsF fm(small);
    if (!m_banner.isEmpty()) {
        // The line at the top, as the mock's toast: what a drag does, and how to stop.
        const QString stop = tr("Escape stops");
        const double width = fm.horizontalAdvance(m_banner) + fm.horizontalAdvance(stop) + 36;
        const double canvasWidth = p.device()->width() / p.device()->devicePixelRatioF();
        const QRectF banner((canvasWidth - width) / 2.0, 12, width, 30);
        drawCard(p, banner);
        p.setFont(small);
        p.setPen(t.text);
        p.drawText(banner.adjusted(12, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, m_banner);
        p.setPen(t.muted);
        p.drawText(banner.adjusted(0, 0, -12, 0), Qt::AlignRight | Qt::AlignVCenter, stop);
    }
    if (m_marker) {
        const QPointF c(photo.left() + (m_marker->x() * photo.width()),
                        photo.top() + (m_marker->y() * photo.height()));
        // A white ring with dark edges inside and out: one of them shows on any colour.
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 150), 1.0));
        p.drawEllipse(c, kMarker + 1.0, kMarker + 1.0);
        p.drawEllipse(c, kMarker - 2.0, kMarker - 2.0);
        p.setPen(QPen(Qt::white, 2.0));
        p.drawEllipse(c, kMarker - 0.5, kMarker - 0.5);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(c, 1.5, 1.5);
        if (!m_title.isEmpty()) {
            QFont bold = small;
            bold.setWeight(QFont::DemiBold);
            const double textWidth =
                std::max(QFontMetricsF(bold).horizontalAdvance(m_title), fm.horizontalAdvance(m_line));
            const QSizeF size(textWidth + 20, 40);
            // Right of the marker, or left of it near the photo's right edge, never over the colour.
            const bool left = c.x() + kHintGap + size.width() > photo.right();
            const QRectF hint(left ? c.x() - kHintGap - size.width() : c.x() + kHintGap,
                              c.y() - (size.height() / 2.0), size.width(), size.height());
            drawCard(p, hint);
            p.setFont(bold);
            p.setPen(t.text);
            p.drawText(hint.adjusted(10, 4, -10, -size.height() / 2.0), Qt::AlignLeft | Qt::AlignVCenter,
                       m_title);
            p.setFont(small);
            p.setPen(t.muted);
            p.drawText(hint.adjusted(10, size.height() / 2.0 - 2, -10, -4), Qt::AlignLeft | Qt::AlignVCenter,
                       m_line);
        }
    }
    p.restore();
}

} // namespace pixelfx::ui
