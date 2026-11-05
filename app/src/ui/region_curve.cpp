#include "ui/region_curve.h"

#include "ui/style.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <pfx/curve.h>
#include <string>

namespace pixelfx::ui {

namespace {

constexpr std::array<const char*, 3> kSplitKeys = {"curves.parametric.split.shadows",
                                                   "curves.parametric.split.midtones",
                                                   "curves.parametric.split.highlights"};
constexpr std::array<double, 3> kDefaults = {25.0, 50.0, 75.0};
constexpr double kMinimum = 5.0;
constexpr double kMaximum = 95.0;
constexpr double kGap = 5.0;      // splits stay this far apart
constexpr double kMarkHalf = 6.0; // half the mark's width; its hit area is 12 px either side

} // namespace

RegionCurveView::RegionCurveView(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(kPlotHeight + kMarkStrip);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(tr("Region splits"));
    setToolTip(tr("Drag a mark to move where two regions meet. Keyboard: Tab to a mark, Left and Right "
                  "move it."));
    refreshDescription();
}

RegionCurveView::~RegionCurveView() = default;

void RegionCurveView::setSettings(const pfx::Settings& settings)
{
    m_settings = settings;
    for (std::size_t i = 0; i < kSplitKeys.size(); ++i) {
        m_splits.at(i) = settings.number(kSplitKeys.at(i), kDefaults.at(i));
    }
    refreshDescription();
    update();
}

double RegionCurveView::split(int index) const
{
    return m_splits.at(static_cast<std::size_t>(index));
}

QRectF RegionCurveView::plot() const
{
    return QRectF(0.5, 0.5, width() - 1.0, kPlotHeight - 1.0).adjusted(6, 6, -6, -6);
}

QPointF RegionCurveView::markPosition(int index) const
{
    const QRectF r = plot();
    return {r.left() + (r.width() * split(index) / 100.0), kPlotHeight + (kMarkStrip / 2.0)};
}

void RegionCurveView::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokens();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF frame(0.5, 0.5, width() - 1.0, kPlotHeight - 1.0);
    p.setPen(t.border);
    p.setBrush(t.input);
    p.drawRoundedRect(frame, 8, 8);
    const QRectF r = plot();
    // Every other region shaded, so the four read at a glance.
    const std::array<double, 5> edges = {0.0, m_splits[0], m_splits[1], m_splits[2], 100.0};
    p.setPen(Qt::NoPen);
    QColor band = t.hover;
    band.setAlphaF(0.6F);
    p.setBrush(band);
    for (const std::size_t k : {0U, 2U}) {
        const double x0 = r.left() + (r.width() * edges.at(k) / 100.0);
        const double x1 = r.left() + (r.width() * edges.at(k + 1) / 100.0);
        p.drawRect(QRectF(x0, r.top(), x1 - x0, r.height()));
    }
    p.setPen(QPen(t.faint, 1, Qt::DashLine));
    p.drawLine(r.bottomLeft(), r.topRight());
    const int samples = std::max(64, static_cast<int>(r.width()));
    const std::vector<double> values = pfx::sampleParametricCurve(m_settings, samples);
    QPainterPath path;
    for (int i = 0; i < samples; ++i) {
        const QPointF point(r.left() + (r.width() * i / (samples - 1)),
                            r.bottom() -
                                (r.height() * std::clamp(values[static_cast<std::size_t>(i)], 0.0, 1.0)));
        i == 0 ? path.moveTo(point) : path.lineTo(point);
    }
    p.setPen(QPen(t.text, 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    const bool ring = showsFocusRing(this);
    for (int i = 0; i < 3; ++i) {
        const QPointF c = markPosition(i);
        QPainterPath mark;
        mark.moveTo(c.x(), c.y() - 5.0);
        mark.lineTo(c.x() + kMarkHalf, c.y() + 4.0);
        mark.lineTo(c.x() - kMarkHalf, c.y() + 4.0);
        mark.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(ring && i == m_focused ? t.accent : t.text);
        p.drawPath(mark);
    }
}

void RegionCurveView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_drag = -1;
    double nearest = 2.0 * kMarkHalf;
    for (int i = 0; i < 3; ++i) {
        const double d = std::abs(event->position().x() - markPosition(i).x());
        if (d <= nearest) {
            nearest = d;
            m_drag = i;
        }
    }
    if (m_drag >= 0) {
        m_focused = m_drag;
        update();
    }
}

void RegionCurveView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_drag >= 0) {
        const QRectF r = plot();
        moveSplit(m_drag, 100.0 * (event->position().x() - r.left()) / r.width(), false);
    }
}

void RegionCurveView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_drag >= 0 && event->button() == Qt::LeftButton) {
        const int index = m_drag;
        m_drag = -1;
        Q_EMIT splitMoved(index, split(index), true);
    }
}

void RegionCurveView::moveSplit(int index, double value, bool finished)
{
    const auto i = static_cast<std::size_t>(index);
    const double low = i == 0 ? kMinimum : m_splits.at(i - 1) + kGap;
    const double high = i == 2 ? kMaximum : m_splits.at(i + 1) - kGap;
    m_splits.at(i) = std::round(std::clamp(value, low, high));
    m_settings.set(kSplitKeys.at(i), m_splits.at(i));
    refreshDescription();
    update();
    Q_EMIT splitMoved(index, m_splits.at(i), finished);
}

void RegionCurveView::keyPressEvent(QKeyEvent* event)
{
    const double step = (event->modifiers() & Qt::ShiftModifier) != 0 ? 10.0 : 1.0;
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        moveSplit(m_focused, split(m_focused) + (event->key() == Qt::Key_Left ? -step : step), false);
        m_keyed = true;
        return;
    }
    QWidget::keyPressEvent(event);
}

void RegionCurveView::keyReleaseEvent(QKeyEvent* event)
{
    // One history step for a held key, not one per repeat.
    if (m_keyed && !event->isAutoRepeat()) {
        m_keyed = false;
        Q_EMIT splitMoved(m_focused, split(m_focused), true);
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void RegionCurveView::focusInEvent(QFocusEvent* event)
{
    if (event->reason() == Qt::TabFocusReason) {
        m_focused = 0;
    } else if (event->reason() == Qt::BacktabFocusReason) {
        m_focused = 2;
    }
    refreshDescription();
    QWidget::focusInEvent(event);
}

bool RegionCurveView::focusNextPrevChild(bool next)
{
    // Tab goes through the three marks before it leaves.
    if (hasFocus() && ((next && m_focused < 2) || (!next && m_focused > 0))) {
        m_focused += next ? 1 : -1;
        refreshDescription();
        update();
        return true;
    }
    return QWidget::focusNextPrevChild(next);
}

void RegionCurveView::refreshDescription()
{
    static const std::array<const char*, 3> names = {
        QT_TRANSLATE_NOOP("RegionCurveView", "Shadows and darks meet at %1"),
        QT_TRANSLATE_NOOP("RegionCurveView", "Darks and lights meet at %1"),
        QT_TRANSLATE_NOOP("RegionCurveView", "Lights and highlights meet at %1")};
    setAccessibleDescription(tr(names.at(static_cast<std::size_t>(m_focused))).arg(qRound(split(m_focused))));
}

} // namespace pixelfx::ui
