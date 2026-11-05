#include "ui/straighten_dial.h"

#include "ui/style.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {
constexpr int kWidth = 420;
constexpr double kRange = 45.0;
} // namespace

StraightenDial::StraightenDial(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::SizeHorCursor);
    setAccessibleName(tr("Straighten"));
    setToolTip(tr("Straighten: drag sideways, or use the arrow keys. Double-click resets."));
    resize(sizeHint());
}

StraightenDial::~StraightenDial() = default;

QSize StraightenDial::sizeHint() const
{
    return {kWidth, fontMetrics().height() + 16};
}

void StraightenDial::setAngle(double degrees)
{
    m_angle = std::clamp(degrees, -kRange, kRange);
    setAccessibleDescription(tr("%1 degrees").arg(m_angle, 0, 'f', 1));
    update();
}

void StraightenDial::change(double degrees)
{
    const double rounded = std::round(std::clamp(degrees, -kRange, kRange) * 10.0) / 10.0;
    if (rounded != m_angle) {
        setAngle(rounded);
        Q_EMIT angleChanged(m_angle);
    }
}

void StraightenDial::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int labelsHeight = fontMetrics().height();
    const QRect labels(0, 0, width(), labelsHeight);
    // Labels at -45, -30, -15, the angle, 15, 30, 45, spread evenly.
    const QList<double> marks = {-45, -30, -15, 0, 15, 30, 45};
    for (const double mark : marks) {
        const auto x = static_cast<int>((mark + kRange) / (2 * kRange) * (width() - 1));
        const bool middle = mark == 0.0;
        QFont f = font();
        f.setPixelSize(11);
        f.setBold(middle);
        p.setFont(f);
        p.setPen(middle ? QColor(Qt::white) : QColor(0xDD, 0xDD, 0xDD, 180));
        const QString text = middle ? u"%1°"_s.arg(m_angle, 0, 'f', 1) : QString::number(mark);
        const int w = p.fontMetrics().horizontalAdvance(text);
        p.drawText(QRect(std::clamp(x - (w / 2), 0, width() - w), labels.top(), w, labelsHeight),
                   Qt::AlignCenter, text);
    }
    // Ticks every degree and a half, and the current angle.
    const int top = labelsHeight + 2;
    p.setPen(QPen(QColor(0x99, 0x99, 0x99), 1));
    for (int x = 0; x < width(); x += 14) {
        p.drawLine(x, top, x, top + 10);
    }
    const double at = (m_angle + kRange) / (2 * kRange) * (width() - 1);
    p.setPen(QPen(Qt::white, 2));
    p.drawLine(QPointF(at, top - 2), QPointF(at, top + 12));
    if (showsFocusRing(this)) {
        p.setPen(QPen(tokens().accent, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);
    }
}

void StraightenDial::mousePressEvent(QMouseEvent* event)
{
    m_pressX = event->position().x();
    m_pressAngle = m_angle;
}

void StraightenDial::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons().testFlag(Qt::LeftButton)) {
        const double perPixel = (2 * kRange) / width();
        change(m_pressAngle + ((event->position().x() - m_pressX) * perPixel));
    }
}

void StraightenDial::mouseDoubleClickEvent(QMouseEvent* /*event*/)
{
    change(0.0);
}

void StraightenDial::keyPressEvent(QKeyEvent* event)
{
    const double step = event->modifiers().testFlag(Qt::ShiftModifier) ? 1.0 : 0.1;
    if (event->key() == Qt::Key_Left) {
        change(m_angle - step);
    } else if (event->key() == Qt::Key_Right) {
        change(m_angle + step);
    } else {
        QWidget::keyPressEvent(event);
    }
}

} // namespace pixelfx::ui
