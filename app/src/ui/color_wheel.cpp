#include "ui/color_wheel.h"

#include "ui/style.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace pixelfx::ui {

namespace {

constexpr double kPuck = 14.0;
constexpr float kRingSaturation = 0.65F;
constexpr float kRingValue = 0.88F;
constexpr int kCentreGrey = 0x7A;

double wrapHue(double hue)
{
    const double h = std::fmod(hue, 360.0);
    return h < 0.0 ? h + 360.0 : h;
}

} // namespace

ColorWheel::ColorWheel(const QString& name, QWidget* parent)
    : QWidget(parent)
    , m_name(name)
{
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(kDiameter, kDiameter);
    setCursor(Qt::CrossCursor);
    setAccessibleName(name);
    setToolTip(tr("%1: drag to pick the colour, further from the centre is stronger. Arrow keys: Left "
                  "and Right turn the hue, Up and Down change the amount. Double-click resets.")
                   .arg(name));
    refreshDescription();
}

ColorWheel::~ColorWheel() = default;

void ColorWheel::setColor(double hue, double amount)
{
    m_hue = wrapHue(hue);
    m_amount = std::clamp(amount, 0.0, 100.0);
    refreshDescription();
    update();
}

QRectF ColorWheel::disc() const
{
    return QRectF(rect()).adjusted(1, 1, -1, -1);
}

QPointF ColorWheel::puckPosition() const
{
    const QRectF d = disc();
    const double radius = (d.width() / 2.0) * (m_amount / 100.0);
    const double a = m_hue * std::numbers::pi / 180.0;
    return {d.center().x() + (radius * std::sin(a)), d.center().y() - (radius * std::cos(a))};
}

std::pair<double, double> ColorWheel::colorAt(const QPointF& position) const
{
    const QRectF d = disc();
    const double dx = position.x() - d.center().x();
    const double dy = position.y() - d.center().y();
    const double distance = std::hypot(dx, dy);
    const double amount = std::round(std::min(1.0, distance / (d.width() / 2.0)) * 100.0);
    if (amount < 1.0) {
        return {m_hue, 0.0}; // the centre has no hue: keep the last one
    }
    const double hue = std::round(wrapHue(std::atan2(dx, -dy) * 180.0 / std::numbers::pi));
    return {wrapHue(hue), amount};
}

void ColorWheel::change(double hue, double amount, bool finished)
{
    setColor(hue, amount);
    Q_EMIT moved(m_hue, m_amount);
    if (finished) {
        Q_EMIT released(m_hue, m_amount);
    }
}

void ColorWheel::refreshDescription()
{
    //: Read by screen readers for a colour grading wheel, for example "hue 200 degrees, amount 20".
    setAccessibleDescription(tr("hue %1 degrees, amount %2").arg(qRound(m_hue)).arg(qRound(m_amount)));
}

void ColorWheel::renderDisc(qreal ratio)
{
    const int size = qRound(kDiameter * ratio);
    m_disc = QImage(size, size, QImage::Format_ARGB32_Premultiplied);
    m_disc.setDevicePixelRatio(ratio);
    m_disc.fill(Qt::transparent);
    const double centre = size / 2.0;
    for (int y = 0; y < size; ++y) {
        auto* line = reinterpret_cast<QRgb*>(m_disc.scanLine(y));
        for (int x = 0; x < size; ++x) {
            const double dx = x + 0.5 - centre;
            const double dy = y + 0.5 - centre;
            const double r = std::hypot(dx, dy) / centre;
            if (r > 1.0) {
                continue;
            }
            const double hue = wrapHue(std::atan2(dx, -dy) * 180.0 / std::numbers::pi);
            const QColor ring =
                QColor::fromHsvF(static_cast<float>(hue / 360.0), kRingSaturation, kRingValue);
            const auto mix = [r](int edge) { return qRound((kCentreGrey * (1.0 - r)) + (edge * r)); };
            line[x] = qRgb(mix(ring.red()), mix(ring.green()), mix(ring.blue()));
        }
    }
}

void ColorWheel::paintEvent(QPaintEvent* /*event*/)
{
    const qreal ratio = devicePixelRatioF();
    if (m_disc.isNull() || !qFuzzyCompare(m_disc.devicePixelRatio(), ratio)) {
        renderDisc(ratio);
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // Clip the disc to a smooth circle; the image edge alone is jagged.
    QPainterPath circle;
    circle.addEllipse(disc());
    p.save();
    p.setClipPath(circle);
    p.drawImage(rect(), m_disc);
    p.restore();
    // The rim keeps the edge visible on the light theme.
    p.setBrush(Qt::NoBrush);
    const bool ring = showsFocusRing(this);
    p.setPen(QPen(ring ? tokens().accent : tokens().faint, ring ? 2.0 : 1.0));
    p.drawEllipse(disc());
    // The puck: white with a dark outer ring, so one of its edges always shows.
    const QPointF c = puckPosition();
    const double r = kPuck / 2.0;
    p.setPen(QPen(QColor(0, 0, 0, 140), 1.0));
    p.drawEllipse(c, r + 0.5, r + 0.5);
    p.setPen(QPen(Qt::white, 2.0));
    p.drawEllipse(c, r - 1.0, r - 1.0);
}

void ColorWheel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_dragging = true;
    const auto [hue, amount] = colorAt(event->position());
    change(hue, amount, false);
}

void ColorWheel::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        const auto [hue, amount] = colorAt(event->position());
        change(hue, amount, false);
    }
}

void ColorWheel::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        Q_EMIT released(m_hue, m_amount);
    }
}

void ColorWheel::mouseDoubleClickEvent(QMouseEvent* /*event*/)
{
    m_dragging = false;
    change(0.0, 0.0, true);
}

void ColorWheel::keyPressEvent(QKeyEvent* event)
{
    const double step = (event->modifiers() & Qt::ShiftModifier) != 0 ? 10.0 : 1.0;
    switch (event->key()) {
    case Qt::Key_Left:
        change(m_hue - step, m_amount, false);
        break;
    case Qt::Key_Right:
        change(m_hue + step, m_amount, false);
        break;
    case Qt::Key_Up:
        change(m_hue, m_amount + step, false);
        break;
    case Qt::Key_Down:
        change(m_hue, m_amount - step, false);
        break;
    case Qt::Key_Backspace:
    case Qt::Key_Delete:
        change(0.0, 0.0, true);
        return;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    m_keyed = true;
}

void ColorWheel::keyReleaseEvent(QKeyEvent* event)
{
    // One history step for a held key, not one per repeat.
    if (m_keyed && !event->isAutoRepeat()) {
        m_keyed = false;
        Q_EMIT released(m_hue, m_amount);
        return;
    }
    QWidget::keyReleaseEvent(event);
}

} // namespace pixelfx::ui
