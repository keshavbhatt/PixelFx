#include "ui/corner_tool.h"

#include "ui/style.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr double kHandle = 9.0;        ///< a handle's radius
constexpr double kSelectedRing = 14.0; ///< the selected handle's ring
constexpr double kReach = 16.0;        ///< a press this near a corner takes it (a 32 px target)
constexpr double kLens = 78.0;         ///< the magnifier's radius
constexpr double kZoom = 3.0;
constexpr double kLensAway = 200.0; ///< how far the magnifier's middle sits from the corner
constexpr int kDim = 115;           ///< black over the photo outside the shape (45%)

QPointF along(QPointF a, QPointF b, double t)
{
    return a + ((b - a) * t);
}

} // namespace

CornerTool::CornerTool(Canvas* canvas)
    : QObject(canvas)
    , m_canvas(canvas)
{}

CornerTool::~CornerTool() = default;

QString CornerTool::cornerName(int index)
{
    switch (index) {
    case 0:
        return tr("Top left corner");
    case 1:
        return tr("Top right corner");
    case 2:
        return tr("Bottom right corner");
    default:
        return tr("Bottom left corner");
    }
}

void CornerTool::setCorners(const core::Corners& corners)
{
    m_corners = pfx::cornersUsable(corners) ? corners : core::ownCorners();
    m_canvas->update();
}

void CornerTool::reset()
{
    m_corners = core::ownCorners();
    m_canvas->update();
    Q_EMIT cornersChanged(m_corners);
}

void CornerTool::showDragging(int index)
{
    select(index);
    m_dragging = index;
    m_canvas->update();
}

QPointF CornerTool::onScreen(const pfx::Corner& corner, const QRectF& photo)
{
    return {photo.left() + (corner.x * photo.width()), photo.top() + (corner.y * photo.height())};
}

void CornerTool::select(int index)
{
    m_selected = index;
    m_canvas->setAccessibleDescription(
        tr("%1 selected; the arrow keys move it, Tab picks the next").arg(cornerName(index)));
}

bool CornerTool::moveCorner(int index, QPointF fraction)
{
    core::Corners next = m_corners;
    next.at(static_cast<std::size_t>(index)) = {std::clamp(fraction.x(), 0.0, 1.0),
                                                std::clamp(fraction.y(), 0.0, 1.0)};
    if (!pfx::cornersUsable(next)) {
        return false; // the shape would fold or a side vanish: the corner stays where it was
    }
    m_corners = next;
    return true;
}

void CornerTool::paint(QPainter& painter, const QRectF& photo)
{
    std::array<QPointF, 4> p{};
    for (std::size_t i = 0; i < 4; ++i) {
        p.at(i) = onScreen(m_corners.at(i), photo);
    }
    const QPolygonF shape({p[0], p[1], p[2], p[3]});
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath outside;
    outside.setFillRule(Qt::OddEvenFill);
    outside.addRect(photo);
    outside.addPolygon(shape);
    outside.closeSubpath();
    painter.fillPath(outside, QColor(0, 0, 0, kDim));
    painter.setPen(QPen(QColor(255, 255, 255, 115), 1.0));
    for (const double t : {1.0 / 3.0, 2.0 / 3.0}) {
        painter.drawLine(along(p[0], p[1], t), along(p[3], p[2], t));
        painter.drawLine(along(p[0], p[3], t), along(p[1], p[2], t));
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0, 0, 0, 100), 4.0));
    painter.drawPolygon(shape);
    painter.setPen(QPen(Qt::white, 2.0));
    painter.drawPolygon(shape);
    for (std::size_t i = 0; i < 4; ++i) {
        if (std::cmp_equal(i, m_selected)) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(tokens().accent, 4.0));
            painter.drawEllipse(p.at(i), kSelectedRing, kSelectedRing);
        }
        painter.setPen(QPen(QColor(0, 0, 0, 140), 2.0));
        painter.setBrush(Qt::white);
        painter.drawEllipse(p.at(i), kHandle, kHandle);
    }
    painter.restore();
    if (m_dragging >= 0) {
        paintMagnifier(painter, photo);
    }
}

void CornerTool::paintMagnifier(QPainter& painter, const QRectF& photo) const
{
    const QImage& image = m_canvas->image();
    if (image.isNull()) {
        return;
    }
    const QPointF corner = onScreen(m_corners.at(static_cast<std::size_t>(m_dragging)), photo);
    // Beside the corner, towards the photo's middle, never over the corner or off the canvas.
    QPointF away = photo.center() - corner;
    const double length = std::hypot(away.x(), away.y());
    away = length > 1.0 ? away / length : QPointF(1.0, 1.0) / std::numbers::sqrt2;
    QPointF middle = corner + (away * kLensAway);
    const QRectF room = QRectF(m_canvas->rect()).adjusted(kLens + 4, kLens + 4, -kLens - 4, -kLens - 4);
    middle = {std::clamp(middle.x(), room.left(), std::max(room.left(), room.right())),
              std::clamp(middle.y(), room.top(), std::max(room.top(), room.bottom()))};
    const QRectF lens(middle.x() - kLens, middle.y() - kLens, 2 * kLens, 2 * kLens);
    // The photo under the pointer, three times as large: its own pixels, from the picture shown.
    const double sx = image.width() / photo.width();
    const double sy = image.height() / photo.height();
    const double reach = kLens / kZoom;
    const QRectF source((corner.x() - photo.left() - reach) * sx, (corner.y() - photo.top() - reach) * sy,
                        2 * reach * sx, 2 * reach * sy);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath circle;
    circle.addEllipse(lens);
    painter.setClipPath(circle);
    painter.fillRect(lens, m_canvas->backdrop());
    painter.drawImage(lens, image, source);
    painter.setClipping(false);
    painter.setPen(QPen(Qt::white, 3.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(lens);
    painter.setPen(QPen(tokens().accent, 2.0));
    painter.drawLine(QPointF(middle.x() - 14, middle.y()), QPointF(middle.x() + 14, middle.y()));
    painter.drawLine(QPointF(middle.x(), middle.y() - 14), QPointF(middle.x(), middle.y() + 14));
    painter.restore();
}

bool CornerTool::press(const QMouseEvent& event, const QRectF& photo)
{
    if (event.button() != Qt::LeftButton) {
        return false;
    }
    int nearest = -1;
    double best = kReach;
    for (std::size_t i = 0; i < 4; ++i) {
        const QPointF d = event.position() - onScreen(m_corners.at(i), photo);
        const double distance = std::hypot(d.x(), d.y());
        if (distance <= best) {
            best = distance;
            nearest = static_cast<int>(i);
        }
    }
    if (nearest >= 0) {
        select(nearest);
        m_dragging = nearest;
    }
    return true; // the crop box underneath is not dragged while corners are placed
}

void CornerTool::move(const QMouseEvent& event, const QRectF& photo)
{
    if (m_dragging < 0 || photo.isEmpty()) {
        return;
    }
    moveCorner(m_dragging, {(event.position().x() - photo.left()) / photo.width(),
                            (event.position().y() - photo.top()) / photo.height()});
}

void CornerTool::release(const QMouseEvent& /*event*/, const QRectF& /*photo*/)
{
    if (m_dragging >= 0) {
        m_dragging = -1;
        Q_EMIT cornersChanged(m_corners);
    }
}

bool CornerTool::takesKey(const QKeyEvent& event) const
{
    switch (event.key()) {
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return true;
    default:
        return false;
    }
}

void CornerTool::key(const QKeyEvent& event, const QRectF& photo)
{
    if (event.key() == Qt::Key_Tab || event.key() == Qt::Key_Backtab) {
        select((m_selected + (event.key() == Qt::Key_Tab ? 1 : 3)) % 4);
        return;
    }
    if (photo.isEmpty()) {
        return;
    }
    const double step = event.modifiers().testFlag(Qt::ShiftModifier) ? 10.0 : 1.0;
    QPointF delta;
    switch (event.key()) {
    case Qt::Key_Left:
        delta = {-step, 0.0};
        break;
    case Qt::Key_Right:
        delta = {step, 0.0};
        break;
    case Qt::Key_Up:
        delta = {0.0, -step};
        break;
    default:
        delta = {0.0, step};
        break;
    }
    const pfx::Corner& c = m_corners.at(static_cast<std::size_t>(m_selected));
    if (moveCorner(m_selected, {c.x + (delta.x() / photo.width()), c.y + (delta.y() / photo.height())})) {
        Q_EMIT cornersChanged(m_corners);
    }
}

} // namespace pixelfx::ui
