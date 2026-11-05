#include "ui/histogram_view.h"

#include "ui/style.h"

#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace pixelfx::ui {

HistogramView::HistogramView(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(92);
    setAccessibleName(tr("Histogram"));
}

HistogramView::~HistogramView() = default;

void HistogramView::setHistogram(const core::Histogram& histogram)
{
    m_histogram = histogram;
    update();
}

void HistogramView::setCaption(const QString& caption)
{
    m_caption = caption;
    update();
}

void HistogramView::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokens();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF well = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(t.border);
    p.setBrush(t.input);
    p.drawRoundedRect(well, 8, 8);
    if (m_histogram.isEmpty()) {
        return;
    }
    const QRectF area = well.adjusted(4, 6, -4, -4);
    const double top = std::sqrt(static_cast<double>(m_histogram.peak()));
    const QColor colours[3] = {QColor(0xE0, 0x50, 0x50, 115), QColor(0x50, 0xC0, 0x60, 115),
                               QColor(0x50, 0x80, 0xF0, 115)};
    p.setPen(Qt::NoPen);
    // Overlapping channels add up to grey: adding light on a dark well, multiplying on a light one
    // (adding would vanish into white).
    const bool lightWell = t.input.lightness() > 128;
    p.setCompositionMode(lightWell ? QPainter::CompositionMode_Multiply : QPainter::CompositionMode_Plus);
    for (std::size_t c = 0; c < 3; ++c) {
        QPainterPath path(QPointF(area.left(), area.bottom()));
        for (std::size_t i = 0; i < 256; ++i) {
            const double h = std::min(1.0, std::sqrt(static_cast<double>(m_histogram.rgb[c][i])) / top);
            const double x = area.left() + area.width() * static_cast<double>(i) / 255.0;
            path.lineTo(x, area.bottom() - h * area.height());
        }
        path.lineTo(area.right(), area.bottom());
        path.closeSubpath();
        QColor colour = colours[c];
        if (lightWell) {
            colour.setAlpha(200);
        }
        p.setBrush(colour);
        p.drawPath(path);
    }
    if (!m_caption.isEmpty()) {
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        QFont small = font();
        small.setPixelSize(10);
        p.setFont(small);
        p.setPen(t.muted);
        p.drawText(well.adjusted(8, 0, -8, -4), Qt::AlignLeft | Qt::AlignBottom, m_caption);
    }
}

} // namespace pixelfx::ui
