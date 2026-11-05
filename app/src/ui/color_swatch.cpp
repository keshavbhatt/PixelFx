#include "ui/color_swatch.h"

#include "ui/style.h"

#include <QConicalGradient>
#include <QPainter>

namespace pixelfx::ui {

namespace {
constexpr int kSize = 24;
constexpr qreal kRing = 2.0;
} // namespace

ColorSwatch::ColorSwatch(const QBrush& fill, const QString& name, QWidget* parent)
    : QToolButton(parent)
    , m_fill(fill)
{
    setCheckable(true);
    setFixedSize(kSize, kSize);
    setCursor(Qt::PointingHandCursor);
    setToolTip(name);
    setAccessibleName(name);
}

ColorSwatch::~ColorSwatch() = default;

QBrush ColorSwatch::allHues()
{
    QConicalGradient hues(kSize / 2.0, kSize / 2.0, 0);
    const QColor stops[] = {Qt::red, Qt::magenta, Qt::blue, Qt::cyan, Qt::green, Qt::yellow, Qt::red};
    for (int i = 0; i < 7; ++i) {
        hues.setColorAt(i / 6.0, stops[i]);
    }
    return {hues};
}

void ColorSwatch::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF outer = QRectF(rect()).adjusted(kRing / 2, kRing / 2, -kRing / 2, -kRing / 2);
    if (isChecked() || hasFocus() || underMouse()) {
        QColor ring = tokens().text;
        if (!isChecked()) {
            ring.setAlpha(110);
        }
        painter.setPen(QPen(ring, kRing));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(outer);
    }
    // The colour sits inside the ring, 1 px apart, with a hairline that keeps white and black
    // visible on any panel.
    const QRectF inner = QRectF(rect()).adjusted(kRing + 1, kRing + 1, -kRing - 1, -kRing - 1);
    painter.setPen(QPen(tokens().border, 1.0));
    painter.setBrush(m_fill);
    painter.drawEllipse(inner);
}

} // namespace pixelfx::ui
