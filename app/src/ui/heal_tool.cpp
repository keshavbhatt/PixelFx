#include "ui/heal_tool.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr double kSizeStep = 1.25;   // [ and ]
constexpr double kMinStrokePx = 3.0; // brush points closer than this on screen add nothing

double longEdge(const QRectF& photo)
{
    return std::max(photo.width(), photo.height());
}

// White with a dark outline, so a circle reads on any photo (review: 15:1 on a pale sky).
void outlined(QPainter& painter, const std::function<void()>& draw, Qt::PenStyle style = Qt::SolidLine)
{
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0, 0, 0, 170), 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    draw();
    painter.setPen(QPen(Qt::white, 1.5, style, Qt::RoundCap, Qt::RoundJoin));
    draw();
}

} // namespace

HealTool::HealTool(QObject* parent)
    : QObject(parent)
{}

HealTool::~HealTool() = default;

void HealTool::setSpots(const pfx::Spots& spots)
{
    m_spots = spots;
    if (std::cmp_greater_equal(m_selected, m_spots.size())) {
        select(-1);
    }
}

void HealTool::select(int index)
{
    const int next = index >= 0 && std::cmp_less(index, m_spots.size()) ? index : -1;
    if (next != m_selected) {
        m_selected = next;
        Q_EMIT selectionChanged(m_selected);
        Q_EMIT brushChanged(); // the sliders show the selected spot's values
    }
}

double HealTool::size() const
{
    return m_selected >= 0 ? m_spots[static_cast<std::size_t>(m_selected)].radius * 2.0 : m_size;
}

double HealTool::feather() const
{
    return m_selected >= 0 ? m_spots[static_cast<std::size_t>(m_selected)].feather : m_feather;
}

double HealTool::opacity() const
{
    return m_selected >= 0 ? m_spots[static_cast<std::size_t>(m_selected)].opacity : m_opacity;
}

void HealTool::changeSelected(const std::function<void(pfx::Spot&)>& change, bool finished,
                              const QString& label)
{
    change(m_spots[static_cast<std::size_t>(m_selected)]);
    Q_EMIT spotsChanged(m_spots, finished, label);
    Q_EMIT brushChanged();
}

void HealTool::setSize(double size, bool finished)
{
    size = std::clamp(size, kMinSize, kMaxSize);
    if (m_selected < 0) {
        m_size = size;
        Q_EMIT brushChanged();
        return;
    }
    changeSelected([size](pfx::Spot& s) { s.radius = size / 2.0; }, finished, tr("Spot size"));
}

void HealTool::setFeather(double feather, bool finished)
{
    feather = std::clamp(feather, 0.0, 100.0);
    if (m_selected < 0) {
        m_feather = feather;
        Q_EMIT brushChanged();
        return;
    }
    changeSelected([feather](pfx::Spot& s) { s.feather = feather; }, finished, tr("Spot feather"));
}

void HealTool::setOpacity(double opacity, bool finished)
{
    opacity = std::clamp(opacity, 1.0, 100.0);
    if (m_selected < 0) {
        m_opacity = opacity;
        Q_EMIT brushChanged();
        return;
    }
    changeSelected([opacity](pfx::Spot& s) { s.opacity = opacity; }, finished, tr("Spot opacity"));
}

void HealTool::resizeStep(bool larger)
{
    setSize(larger ? size() * kSizeStep : size() / kSizeStep, true);
}

void HealTool::nudge(QPointF delta)
{
    if (m_selected < 0) {
        return;
    }
    changeSelected(
        [delta](pfx::Spot& s) {
            const auto shift = [&delta](double& x, double& y) {
                x = std::clamp(x + delta.x(), 0.0, 1.0);
                y = std::clamp(y + delta.y(), 0.0, 1.0);
            };
            shift(s.x, s.y);
            shift(s.sourceX, s.sourceY);
            for (pfx::CurvePoint& p : s.stroke) {
                shift(p.x, p.y);
            }
        },
        true, tr("Move spot"));
}

void HealTool::addSpotAt(QPointF fraction)
{
    pfx::Spot spot;
    spot.x = std::clamp(fraction.x(), 0.0, 1.0);
    spot.y = std::clamp(fraction.y(), 0.0, 1.0);
    spot.sourceX = spot.x;
    spot.sourceY = spot.y;
    spot.radius = m_size / 2.0;
    spot.feather = m_feather;
    spot.opacity = m_opacity;
    addSpot(std::move(spot));
}

void HealTool::removeSelected()
{
    if (m_selected < 0) {
        return;
    }
    m_spots.erase(m_spots.begin() + m_selected);
    m_selected = -1;
    Q_EMIT selectionChanged(-1);
    Q_EMIT spotsChanged(m_spots, true, tr("Remove spot"));
    Q_EMIT brushChanged();
}

void HealTool::removeAll()
{
    if (m_spots.empty()) {
        return;
    }
    m_spots.clear();
    select(-1);
    Q_EMIT spotsChanged(m_spots, true, tr("Remove all spots"));
}

void HealTool::setShowSpots(bool on)
{
    m_show = on;
}

QPointF HealTool::toFraction(QPointF pos, const QRectF& photo)
{
    return {std::clamp((pos.x() - photo.left()) / photo.width(), 0.0, 1.0),
            std::clamp((pos.y() - photo.top()) / photo.height(), 0.0, 1.0)};
}

QPointF HealTool::toWidget(QPointF fraction, const QRectF& photo)
{
    return {photo.left() + (fraction.x() * photo.width()), photo.top() + (fraction.y() * photo.height())};
}

double HealTool::radiusOnScreen(const pfx::Spot& spot, const QRectF& photo)
{
    return spot.radius * longEdge(photo);
}

int HealTool::spotAt(QPointF pos, const QRectF& photo) const
{
    // The newest on top, so it is found first.
    for (int i = static_cast<int>(m_spots.size()) - 1; i >= 0; --i) {
        const pfx::Spot& s = m_spots[static_cast<std::size_t>(i)];
        const double r = std::max(6.0, radiusOnScreen(s, photo));
        if (s.stroke.size() >= 2) {
            for (const pfx::CurvePoint& p : s.stroke) {
                const QPointF d = toWidget({p.x, p.y}, photo) - pos;
                if (std::hypot(d.x(), d.y()) <= r) {
                    return i;
                }
            }
        } else {
            const QPointF d = toWidget({s.x, s.y}, photo) - pos;
            if (std::hypot(d.x(), d.y()) <= r) {
                return i;
            }
        }
    }
    return -1;
}

bool HealTool::onSource(QPointF pos, const QRectF& photo) const
{
    if (m_selected < 0 || !m_show) {
        return false;
    }
    const pfx::Spot& s = m_spots[static_cast<std::size_t>(m_selected)];
    const QPointF d = toWidget({s.sourceX, s.sourceY}, photo) - pos;
    return std::hypot(d.x(), d.y()) <= std::max(6.0, radiusOnScreen(s, photo));
}

void HealTool::addSpot(pfx::Spot spot)
{
    if (m_finder) {
        spot = m_finder(spot);
    }
    m_spots.push_back(std::move(spot));
    m_selected = static_cast<int>(m_spots.size()) - 1;
    Q_EMIT selectionChanged(m_selected);
    Q_EMIT spotsChanged(m_spots, true,
                        m_spots.back().stroke.empty() ? tr("Heal spot") : tr("Heal brush stroke"));
    Q_EMIT brushChanged();
}

bool HealTool::press(const QMouseEvent& event, const QRectF& photo)
{
    if (photo.isEmpty()) {
        return false;
    }
    m_lastPhoto = photo;
    const QPointF pos = event.position();
    m_pressFraction = toFraction(pos, photo);
    if (onSource(pos, photo)) {
        m_drag = Drag::Source;
        m_pressSpot = m_spots[static_cast<std::size_t>(m_selected)];
        return true;
    }
    if (const int hit = m_show ? spotAt(pos, photo) : -1; hit >= 0) {
        select(hit);
        m_drag = Drag::Target;
        m_pressSpot = m_spots[static_cast<std::size_t>(hit)];
        return true;
    }
    if (!photo.contains(pos)) {
        return false;
    }
    select(-1);
    m_drag = Drag::Brush;
    m_stroke = {m_pressFraction};
    return true;
}

void HealTool::move(const QMouseEvent& event, const QRectF& photo)
{
    m_hover = event.position();
    m_lastPhoto = photo;
    if (m_drag == Drag::None || photo.isEmpty()) {
        return;
    }
    const QPointF at = toFraction(event.position(), photo);
    const QPointF delta = at - m_pressFraction;
    if (m_drag == Drag::Brush) {
        const QPointF last = toWidget(m_stroke.back(), photo);
        if (std::hypot(last.x() - event.position().x(), last.y() - event.position().y()) >= kMinStrokePx) {
            m_stroke.push_back(at);
        }
        return;
    }
    pfx::Spot moved = m_pressSpot;
    const auto shift = [&delta](double& x, double& y) {
        x = std::clamp(x + delta.x(), 0.0, 1.0);
        y = std::clamp(y + delta.y(), 0.0, 1.0);
    };
    shift(moved.sourceX, moved.sourceY);
    if (m_drag == Drag::Target) {
        shift(moved.x, moved.y);
        for (pfx::CurvePoint& p : moved.stroke) {
            shift(p.x, p.y);
        }
    }
    m_spots[static_cast<std::size_t>(m_selected)] = moved;
    Q_EMIT spotsChanged(m_spots, false, {});
}

void HealTool::release(const QMouseEvent& event, const QRectF& photo)
{
    move(event, photo);
    const Drag drag = std::exchange(m_drag, Drag::None);
    if (drag == Drag::Target || drag == Drag::Source) {
        if (m_spots[static_cast<std::size_t>(m_selected)] != m_pressSpot) {
            Q_EMIT spotsChanged(m_spots, true,
                                drag == Drag::Target ? tr("Move spot") : tr("Choose another patch"));
        }
        return;
    }
    if (drag != Drag::Brush || m_stroke.empty()) {
        return;
    }
    pfx::Spot spot;
    spot.x = m_stroke.front().x();
    spot.y = m_stroke.front().y();
    spot.sourceX = spot.x;
    spot.sourceY = spot.y;
    spot.radius = m_size / 2.0;
    spot.feather = m_feather;
    spot.opacity = m_opacity;
    // A short drag is a click: a spot. Spot mode always makes a spot where it was pressed.
    double length = 0;
    for (std::size_t i = 1; i < m_stroke.size(); ++i) {
        const QPointF a = toWidget(m_stroke[i - 1], photo);
        const QPointF b = toWidget(m_stroke[i], photo);
        length += std::hypot(b.x() - a.x(), b.y() - a.y());
    }
    if (m_mode == Mode::Brush && length > radiusOnScreen(spot, photo)) {
        for (const QPointF& p : m_stroke) {
            spot.stroke.push_back({p.x(), p.y()});
        }
    }
    m_stroke.clear();
    addSpot(std::move(spot));
}

void HealTool::leave()
{
    m_hover = {-1, -1};
}

void HealTool::paintSpot(QPainter& painter, const pfx::Spot& spot, const QRectF& photo, bool selected)
{
    const double r = radiusOnScreen(spot, photo);
    const QPointF target = toWidget({spot.x, spot.y}, photo);
    const QPointF offset = toWidget({spot.sourceX, spot.sourceY}, photo) - target;
    QPainterPath shape;
    if (spot.stroke.size() >= 2) {
        // The stroke's outline: its centre line widened to the brush.
        QPainterPath line(toWidget({spot.stroke.front().x, spot.stroke.front().y}, photo));
        for (const pfx::CurvePoint& p : spot.stroke) {
            line.lineTo(toWidget({p.x, p.y}, photo));
        }
        QPainterPathStroker stroker;
        stroker.setWidth(2.0 * r);
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        shape = stroker.createStroke(line).simplified();
    } else {
        shape.addEllipse(target, r, r);
    }
    outlined(painter, [&] { painter.drawPath(shape); });
    if (!selected) {
        return;
    }
    // The selected spot: a fill to say which, its source hollow, and the line between.
    painter.fillPath(shape, QColor(255, 255, 255, 40));
    const QPainterPath source = shape.translated(offset);
    outlined(painter, [&] { painter.drawPath(source); }, Qt::DashLine);
    const QPointF from = target;
    const QPointF to = target + offset;
    const double distance = std::hypot(offset.x(), offset.y());
    if (spot.stroke.size() < 2 && distance > 2.0 * r) {
        const QPointF unit = offset / distance;
        outlined(painter, [&] { painter.drawLine(from + (unit * r), to - (unit * r)); }, Qt::DashLine);
    }
}

void HealTool::paint(QPainter& painter, const QRectF& photo)
{
    m_lastPhoto = photo;
    painter.setRenderHint(QPainter::Antialiasing);
    if (m_show) {
        for (std::size_t i = 0; i < m_spots.size(); ++i) {
            paintSpot(painter, m_spots[i], photo, std::cmp_equal(i, m_selected));
        }
    }
    // The stroke being painted, and the brush outline under the pointer.
    const double r = (m_size / 2.0) * longEdge(photo);
    if (m_drag == Drag::Brush && m_stroke.size() >= 2) {
        QPainterPath line(toWidget(m_stroke.front(), photo));
        for (const QPointF& p : m_stroke) {
            line.lineTo(toWidget(p, photo));
        }
        painter.setPen(QPen(QColor(255, 255, 255, 90), 2.0 * r, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(line);
    }
    if (m_hover.x() >= 0 && m_drag == Drag::None && photo.contains(m_hover)) {
        outlined(painter, [&] { painter.drawEllipse(m_hover, r, r); });
    }
}

} // namespace pixelfx::ui
