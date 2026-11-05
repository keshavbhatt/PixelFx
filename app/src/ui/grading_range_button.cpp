#include "ui/grading_range_button.h"

#include "ui/color_wheel.h"
#include "ui/style.h"

#include <QPainter>
#include <QPainterPath>

namespace pixelfx::ui {

namespace {

constexpr int kPadding = 6;   // inside the rounded highlight, above the wheel and under the name
constexpr double kPuck = 8.0; // the small wheel's puck
constexpr double kRadius = 8.0;

} // namespace

GradingRangeButton::GradingRangeButton(const QString& name, QWidget* parent)
    : QAbstractButton(parent)
    , m_name(name)
{
    setCheckable(true);
    setAutoExclusive(true);
    setFocusPolicy(Qt::StrongFocus);
    setText(name);
    setAccessibleName(name);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setColor(0.0, 0.0);
}

GradingRangeButton::~GradingRangeButton() = default;

void GradingRangeButton::setColor(double hue, double amount)
{
    m_hue = hue;
    m_amount = amount;
    //: A colour grading range and its colour, for example "Midtones: hue 40 degrees, amount 12".
    setToolTip(tr("%1: hue %2 degrees, amount %3").arg(m_name).arg(qRound(hue)).arg(qRound(amount)));
    setAccessibleDescription(toolTip());
    update();
}

QSize GradingRangeButton::sizeHint() const
{
    const int text = fontMetrics().height();
    return {kWheel + (2 * kPadding), kPadding + kWheel + kGapSmall + text + kPadding};
}

void GradingRangeButton::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Tokens& t = tokens();
    const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    if (isChecked()) {
        p.setPen(Qt::NoPen);
        p.setBrush(t.accentSoft);
        p.drawRoundedRect(frame, kRadius, kRadius);
    }
    if (showsFocusRing(this)) {
        p.setPen(QPen(t.accent, 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(frame.adjusted(1, 1, -1, -1), kRadius, kRadius);
    }
    const QRectF disc((width() - kWheel) / 2.0, kPadding, kWheel, kWheel);
    paintColorWheel(p, disc, m_disc, m_hue, m_amount, kPuck, false);
    QFont f = font();
    f.setPixelSize(12);
    f.setBold(isChecked());
    p.setFont(f);
    p.setPen(isChecked() ? t.accent : t.muted);
    const QRectF label(0, disc.bottom() + kGapSmall, width(), height() - disc.bottom() - kGapSmall);
    p.drawText(label, Qt::AlignHCenter | Qt::AlignTop, m_name);
}

} // namespace pixelfx::ui
