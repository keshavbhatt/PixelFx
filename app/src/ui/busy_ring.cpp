#include "ui/busy_ring.h"

#include "ui/style.h"

#include <QPainter>
#include <QTimer>

namespace pixelfx::ui {

namespace {

constexpr int kFrame = 16; // ms
constexpr int kTurn = 900; // ms for a full turn
constexpr double kPen = 2.0;

} // namespace

BusyRing::BusyRing(QWidget* parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
{
    setFixedSize(kSize, kSize);
    m_timer->setInterval(kFrame);
    connect(m_timer, &QTimer::timeout, this, [this] {
        m_angle = (m_angle + (360 * kFrame / kTurn)) % 360;
        update();
    });
}

BusyRing::~BusyRing() = default;

void BusyRing::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_timer->start();
}

void BusyRing::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_timer->stop();
}

void BusyRing::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokens();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(kPen / 2, kPen / 2, -kPen / 2, -kPen / 2);
    p.setPen(QPen(t.border, kPen));
    p.drawEllipse(r);
    // A quarter arc turning.
    p.setPen(QPen(t.text, kPen, Qt::SolidLine, Qt::RoundCap));
    constexpr int kSixteenths = 16; // QPainter angles are in 1/16 of a degree
    p.drawArc(r, (90 - m_angle) * kSixteenths, -90 * kSixteenths);
}

} // namespace pixelfx::ui
