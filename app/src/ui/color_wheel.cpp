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

QImage renderDisc(int diameter, qreal ratio)
{
    const int size = qRound(diameter * ratio);
    QImage disc(size, size, QImage::Format_ARGB32_Premultiplied);
    disc.setDevicePixelRatio(ratio);
    disc.fill(Qt::transparent);
    const double centre = size / 2.0;
    for (int y = 0; y < size; ++y) {
        auto* line = reinterpret_cast<QRgb*>(disc.scanLine(y));
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
    return disc;
}

} // namespace

void paintColorWheel(QPainter& painter, const QRectF& disc, QImage& cache, double hue, double amount,
                     double puck, bool focused)
{
    const qreal ratio = painter.device()->devicePixelRatioF();
    const int diameter = qRound(disc.width());
    if (cache.isNull() || !qFuzzyCompare(cache.devicePixelRatio(), ratio) ||
        qRound(cache.width() / ratio) != diameter) {
        cache = renderDisc(diameter, ratio);
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    // Clip the disc to a smooth circle; the image edge alone is jagged.
    QPainterPath circle;
    circle.addEllipse(disc);
    painter.save();
    painter.setClipPath(circle);
    painter.drawImage(disc, cache);
    painter.restore();
    // The rim keeps the edge visible on the light theme.
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(focused ? tokens().accent : tokens().faint, focused ? 2.0 : 1.0));
    painter.drawEllipse(disc);
    // The puck: white with a dark outer ring, so one of its edges always shows.
    const double radius = (disc.width() / 2.0) * (std::clamp(amount, 0.0, 100.0) / 100.0);
    const double a = hue * std::numbers::pi / 180.0;
    const QPointF c(disc.center().x() + (radius * std::sin(a)), disc.center().y() - (radius * std::cos(a)));
    const double r = puck / 2.0;
    painter.setPen(QPen(QColor(0, 0, 0, 140), 1.0));
    painter.drawEllipse(c, r + 0.5, r + 0.5);
    painter.setPen(QPen(Qt::white, puck >= 10.0 ? 2.0 : 1.5));
    painter.drawEllipse(c, r - 1.0, r - 1.0);
    painter.restore();
}

ColorWheel::ColorWheel(const QString& name, QWidget* parent, int diameter)
    : QWidget(parent)
    , m_diameter(diameter)
{
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(diameter, diameter);
    setCursor(Qt::CrossCursor);
    setName(name);
    refreshDescription();
}

void ColorWheel::setName(const QString& name)
{
    m_name = name;
    setAccessibleName(name);
    setToolTip(tr("%1: drag to pick the colour, further from the centre is stronger. Arrow keys: Left "
                  "and Right turn the hue, Up and Down change the amount. Double-click resets.")
                   .arg(name));
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

void ColorWheel::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    paintColorWheel(p, disc(), m_disc, m_hue, m_amount, kPuck, showsFocusRing(this));
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
