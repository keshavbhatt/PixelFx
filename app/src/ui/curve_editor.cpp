#include "ui/curve_editor.h"

#include "ui/style.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <pfx/curve.h>

namespace pixelfx::ui {

namespace {
constexpr double kHitRadius = 8.0;
constexpr double kMinGap = 0.01; // points keep this far apart in x
constexpr int kMaxPoints = 16;
pfx::Curve identity()
{
    return {{0.0, 0.0}, {1.0, 1.0}};
}
} // namespace

CurveEditor::CurveEditor(QWidget* parent)
    : QWidget(parent)
    , m_points(identity())
    , m_color(tokens().text)
    , m_colorToken(tokenIndex(m_color))
{
    setMinimumHeight(160);
    setMouseTracking(true);
    setAccessibleName(tr("Tone curve"));
    setToolTip(tr("Click to add a point, drag to move it, double-click to remove it."));
}

CurveEditor::~CurveEditor() = default;

void CurveEditor::setCurve(const pfx::Curve& curve)
{
    m_points = curve.size() >= 2 ? curve : identity();
    m_drag = -1;
    update();
}

bool CurveEditor::isIdentity() const
{
    return std::ranges::all_of(m_points, [](const pfx::CurvePoint& p) { return std::abs(p.x - p.y) < 1e-6; });
}

void CurveEditor::setColor(const QColor& color)
{
    m_color = color;
    m_colorToken = tokenIndex(color); // a theme colour follows a theme switch
    update();
}

QRectF CurveEditor::plot() const
{
    return QRectF(rect()).adjusted(6, 6, -6, -6);
}

QPointF CurveEditor::toWidget(const pfx::CurvePoint& p) const
{
    const QRectF r = plot();
    return {r.left() + p.x * r.width(), r.bottom() - p.y * r.height()};
}

pfx::CurvePoint CurveEditor::fromWidget(const QPointF& p) const
{
    const QRectF r = plot();
    return {std::clamp((p.x() - r.left()) / r.width(), 0.0, 1.0),
            std::clamp((r.bottom() - p.y()) / r.height(), 0.0, 1.0)};
}

int CurveEditor::hit(const QPointF& position) const
{
    for (std::size_t i = 0; i < m_points.size(); ++i) {
        const QPointF d = toWidget(m_points[i]) - position;
        if (std::hypot(d.x(), d.y()) <= kHitRadius) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void CurveEditor::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokens();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = plot();
    p.setPen(t.border);
    p.setBrush(t.input);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    for (int i = 1; i < 4; ++i) {
        const double f = i / 4.0;
        p.drawLine(QPointF(r.left() + f * r.width(), r.top()), QPointF(r.left() + f * r.width(), r.bottom()));
        p.drawLine(QPointF(r.left(), r.top() + f * r.height()), QPointF(r.right(), r.top() + f * r.height()));
    }
    p.setPen(QPen(t.faint, 1, Qt::DashLine));
    p.drawLine(r.bottomLeft(), r.topRight());

    const int samples = std::max(64, static_cast<int>(r.width()));
    const std::vector<double> values = pfx::sampleCurve(m_points, samples);
    QPainterPath path;
    for (int i = 0; i < samples; ++i) {
        const pfx::CurvePoint c{static_cast<double>(i) / (samples - 1),
                                std::clamp(values[static_cast<std::size_t>(i)], 0.0, 1.0)};
        i == 0 ? path.moveTo(toWidget(c)) : path.lineTo(toWidget(c));
    }
    const QColor colour = m_colorToken >= 0 ? tokenColor(m_colorToken) : m_color;
    p.setPen(QPen(colour, 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.setPen(QPen(t.input, 1.5));
    p.setBrush(colour);
    for (const pfx::CurvePoint& point : m_points) {
        p.drawEllipse(toWidget(point), 4.5, 4.5);
    }
}

void CurveEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_drag = hit(event->position());
    if (m_drag < 0 && static_cast<int>(m_points.size()) < kMaxPoints) {
        const pfx::CurvePoint added = fromWidget(event->position());
        const auto at =
            std::ranges::find_if(m_points, [&](const pfx::CurvePoint& p) { return p.x > added.x; });
        if (at == m_points.begin() || at == m_points.end() || added.x - std::prev(at)->x < kMinGap ||
            at->x - added.x < kMinGap) {
            return;
        }
        m_drag = static_cast<int>(std::distance(m_points.begin(), m_points.insert(at, added)));
        update();
        Q_EMIT curveChanged(m_points);
    }
}

void CurveEditor::moveDragged(const QPointF& position)
{
    const auto i = static_cast<std::size_t>(m_drag);
    pfx::CurvePoint p = fromWidget(position);
    if (i == 0 || i + 1 == m_points.size()) {
        p.x = m_points[i].x; // the end points stay at 0 and 1
    } else {
        p.x = std::clamp(p.x, m_points[i - 1].x + kMinGap, m_points[i + 1].x - kMinGap);
    }
    m_points[i] = p;
    update();
    Q_EMIT curveChanged(m_points);
}

void CurveEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (m_drag >= 0 && event->buttons().testFlag(Qt::LeftButton)) {
        moveDragged(event->position());
        return;
    }
    setCursor(hit(event->position()) >= 0 ? Qt::OpenHandCursor : Qt::CrossCursor);
}

void CurveEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_drag >= 0) {
        // The release ends the drag where it happens, even if no move with the button reached us.
        if (fromWidget(event->position()) != m_points[static_cast<std::size_t>(m_drag)]) {
            moveDragged(event->position());
        }
        m_drag = -1;
        Q_EMIT editFinished();
    }
}

void CurveEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int i = hit(event->position());
    if (i > 0 && i + 1 < static_cast<int>(m_points.size())) {
        m_points.erase(m_points.begin() + i);
        m_drag = -1;
        update();
        Q_EMIT curveChanged(m_points);
        Q_EMIT editFinished();
    }
}

} // namespace pixelfx::ui
