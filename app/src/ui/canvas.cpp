#include "ui/canvas.h"

#include "ui/style.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace pixelfx::ui {

namespace {
constexpr int kMargin = 28;
} // namespace

Canvas::Canvas(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAccessibleName(tr("Photo"));
}

Canvas::~Canvas() = default;

void Canvas::setImage(const QImage& image, const QSize& fullSize)
{
    m_image = image;
    m_fullSize = fullSize.isValid() ? fullSize : image.size();
    m_status.clear();
    m_error.clear();
    update();
}

void Canvas::showImage(const QImage& image)
{
    m_image = image;
    update();
}

void Canvas::setCompare(bool on)
{
    m_compare = on;
    setCursor(on ? Qt::SplitHCursor : Qt::ArrowCursor);
    update();
}

void Canvas::setOriginal(const QImage& original)
{
    m_original = original;
    update();
}

void Canvas::setSplit(double split)
{
    m_split = std::clamp(split, 0.0, 1.0);
    update();
}

void Canvas::paintCompare(QPainter& painter, const QRect& target)
{
    const int x = target.left() + static_cast<int>(m_split * target.width());
    painter.save();
    painter.setClipRect(QRect(target.left(), target.top(), x - target.left(), target.height()));
    painter.drawImage(target, m_original);
    painter.restore();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::white, 2));
    painter.drawLine(x, target.top(), x, target.bottom());
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    const QPoint knob(x, target.center().y());
    painter.drawEllipse(knob, 15, 15);
    // A left-right arrow, as in editor-compare.html.
    painter.setBrush(QColor(0x22, 0x22, 0x22));
    painter.drawPolygon(QPolygon({knob + QPoint(-9, 0), knob + QPoint(-3, -5), knob + QPoint(-3, 5)}));
    painter.drawPolygon(QPolygon({knob + QPoint(9, 0), knob + QPoint(3, -5), knob + QPoint(3, 5)}));
    const auto tag = [&painter](const QRect& box, const QString& text) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 140));
        painter.drawRoundedRect(box, 6, 6);
        painter.setPen(Qt::white);
        painter.drawText(box, Qt::AlignCenter, text);
    };
    const QFontMetrics fm(font());
    const QString before = tr("Before");
    const QString after = tr("After");
    tag(QRect(target.left() + 10, target.top() + 10, fm.horizontalAdvance(before) + 16, fm.height() + 6),
        before);
    const int afterWidth = fm.horizontalAdvance(after) + 16;
    tag(QRect(target.right() - 10 - afterWidth, target.top() + 10, afterWidth, fm.height() + 6), after);
}

void Canvas::mousePressEvent(QMouseEvent* event)
{
    if (m_compare && event->button() == Qt::LeftButton && photoRect().isValid()) {
        m_dragging = true;
        mouseMoveEvent(event);
    }
}

void Canvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        const QRect target = photoRect();
        setSplit((event->position().x() - target.left()) / std::max(1, target.width()));
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    m_dragging = false;
}

void Canvas::setStatus(const QString& text)
{
    m_status = text;
    m_error.clear();
    update();
}

void Canvas::setError(const QString& text)
{
    m_error = text;
    m_status.clear();
    update();
}

QRect Canvas::photoRect() const
{
    if (m_image.isNull()) {
        return {};
    }
    const QRect area = rect().adjusted(kMargin, kMargin, -kMargin, -kMargin);
    QSize size = m_image.size();
    size.scale(area.size(), Qt::KeepAspectRatio);
    if (size.width() > m_fullSize.width()) { // never enlarge past 100% of the photo at fit
        size = m_image.size().scaled(m_fullSize, Qt::KeepAspectRatio);
    }
    QRect target(QPoint(0, 0), size);
    target.moveCenter(area.center());
    return target;
}

void Canvas::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokens();
    QPainter painter(this);
    painter.fillRect(rect(), t.canvas);
    if (!m_image.isNull()) {
        const QRect target = photoRect();
        painter.fillRect(target.translated(0, 6).adjusted(-2, -2, 2, 2), QColor(0, 0, 0, 90));
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(target, m_image);
        if (m_compare && !m_original.isNull()) {
            paintCompare(painter, target);
        }
    }
    const QString& line = m_error.isEmpty() ? m_status : m_error;
    if (!line.isEmpty()) {
        painter.setPen(m_error.isEmpty() ? t.muted : t.danger);
        painter.drawText(rect().adjusted(kMargin, kMargin, -kMargin, -kMargin),
                         Qt::AlignCenter | Qt::TextWordWrap, line);
    }
}

} // namespace pixelfx::ui
