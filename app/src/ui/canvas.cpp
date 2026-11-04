#include "ui/canvas.h"

#include "ui/style.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace pixelfx::ui {

namespace {
constexpr int kMargin = 28;
constexpr double kCornerReach = 14.0; ///< how near a corner a press grabs it
constexpr double kEdgeReach = 8.0;
constexpr double kMinimumBox = 24.0;
constexpr int kCornerLength = 18; ///< mock crop.html: 18 px corners, 4 px thick, 3 px outside
constexpr int kCornerWidth = 4;

Qt::CursorShape cursorFor(core::CropHandle handle)
{
    using enum core::CropHandle;
    switch (handle) {
    case TopLeft:
    case BottomRight:
        return Qt::SizeFDiagCursor;
    case TopRight:
    case BottomLeft:
        return Qt::SizeBDiagCursor;
    case Left:
    case Right:
        return Qt::SizeHorCursor;
    case Top:
    case Bottom:
        return Qt::SizeVerCursor;
    case Move:
        return Qt::SizeAllCursor;
    case None:
        break;
    }
    return Qt::ArrowCursor;
}
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

void Canvas::setCropMode(bool on)
{
    m_cropping = on;
    m_handle = core::CropHandle::None;
    setMouseTracking(on);
    setCursor(Qt::ArrowCursor);
    update();
}

void Canvas::setCrop(const QRectF& crop)
{
    m_crop = crop;
    update();
}

void Canvas::setCropRatio(double ratio)
{
    m_cropRatio = ratio;
}

void Canvas::setCropOverlay(CropOverlay overlay)
{
    m_overlay = overlay;
    update();
}

QRectF Canvas::cropBox() const
{
    const QRectF target(photoRect());
    return {target.left() + (m_crop.x() * target.width()), target.top() + (m_crop.y() * target.height()),
            m_crop.width() * target.width(), m_crop.height() * target.height()};
}

core::CropHandle Canvas::cropHandleAt(QPointF pos) const
{
    using enum core::CropHandle;
    const QRectF box = cropBox();
    if (!m_cropping || box.isEmpty()) {
        return None;
    }
    const auto nearPoint = [pos](QPointF corner) {
        return std::abs(pos.x() - corner.x()) <= kCornerReach &&
               std::abs(pos.y() - corner.y()) <= kCornerReach;
    };
    if (nearPoint(box.topLeft())) {
        return TopLeft;
    }
    if (nearPoint(box.topRight())) {
        return TopRight;
    }
    if (nearPoint(box.bottomLeft())) {
        return BottomLeft;
    }
    if (nearPoint(box.bottomRight())) {
        return BottomRight;
    }
    const bool inX = pos.x() >= box.left() && pos.x() <= box.right();
    const bool inY = pos.y() >= box.top() && pos.y() <= box.bottom();
    if (inY && std::abs(pos.x() - box.left()) <= kEdgeReach) {
        return Left;
    }
    if (inY && std::abs(pos.x() - box.right()) <= kEdgeReach) {
        return Right;
    }
    if (inX && std::abs(pos.y() - box.top()) <= kEdgeReach) {
        return Top;
    }
    if (inX && std::abs(pos.y() - box.bottom()) <= kEdgeReach) {
        return Bottom;
    }
    return (inX && inY) ? Move : None;
}

void Canvas::mousePressEvent(QMouseEvent* event)
{
    if (m_cropping) {
        if (event->button() == Qt::LeftButton) {
            m_handle = cropHandleAt(event->position());
            m_pressPos = event->position();
            m_pressBox = cropBox();
        }
        return;
    }
    if (m_compare && event->button() == Qt::LeftButton && photoRect().isValid()) {
        m_dragging = true;
        mouseMoveEvent(event);
    }
}

void Canvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_cropping) {
        if (m_handle == core::CropHandle::None) {
            setCursor(cursorFor(cropHandleAt(event->position())));
            return;
        }
        const QRectF target(photoRect());
        if (target.isEmpty()) {
            return;
        }
        // Shift keeps the box's shape when the ratio is free (mock: the strip's hint).
        const bool keep = m_cropRatio <= 0 && event->modifiers().testFlag(Qt::ShiftModifier);
        const double ratio = keep ? m_pressBox.width() / std::max(1.0, m_pressBox.height()) : m_cropRatio;
        const QRectF box =
            core::dragCrop(m_pressBox, m_handle, event->position() - m_pressPos, target, ratio, kMinimumBox);
        m_crop = QRectF((box.x() - target.x()) / target.width(), (box.y() - target.y()) / target.height(),
                        box.width() / target.width(), box.height() / target.height());
        update();
        Q_EMIT cropEdited(m_crop);
        return;
    }
    if (m_dragging) {
        const QRect target = photoRect();
        setSplit((event->position().x() - target.left()) / std::max(1, target.width()));
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    m_dragging = false;
    m_handle = core::CropHandle::None;
}

void Canvas::paintOverlay(QPainter& painter, const QRectF& box) const
{
    QList<double> stops;
    switch (m_overlay) {
    case CropOverlay::Thirds:
        stops = {1.0 / 3, 2.0 / 3};
        break;
    case CropOverlay::Grid:
        for (int i = 1; i < 6; ++i) {
            stops << i / 6.0;
        }
        break;
    case CropOverlay::Golden:
        stops = {0.382, 0.618};
        break;
    case CropOverlay::Diagonal: {
        const double d = std::min(box.width(), box.height());
        painter.drawLine(box.topLeft(), box.topLeft() + QPointF(d, d));
        painter.drawLine(box.topRight(), box.topRight() + QPointF(-d, d));
        painter.drawLine(box.bottomLeft(), box.bottomLeft() + QPointF(d, -d));
        painter.drawLine(box.bottomRight(), box.bottomRight() + QPointF(-d, -d));
        return;
    }
    case CropOverlay::None:
        return;
    }
    for (const double f : stops) {
        const double x = box.left() + (f * box.width());
        const double y = box.top() + (f * box.height());
        painter.drawLine(QPointF(x, box.top()), QPointF(x, box.bottom()));
        painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
    }
}

void Canvas::paintCrop(QPainter& painter, const QRect& target)
{
    const QRectF box = cropBox();
    // The rest of the photo dims (mock: brightness .45).
    QPainterPath outside;
    outside.addRect(QRectF(target));
    outside.addRect(box);
    painter.fillPath(outside, QColor(0, 0, 0, 140));
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(QColor(255, 255, 255, 140), 1));
    paintOverlay(painter, box);
    painter.setPen(QPen(QColor(0, 0, 0, 100), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(box.adjusted(-1, -1, 1, 1));
    painter.setPen(QPen(Qt::white, 1));
    painter.drawRect(box);
    // Corner handles.
    const QRect b = box.toAlignedRect();
    constexpr int kOut = 3;
    constexpr int kLen = kCornerLength;
    constexpr int kWide = kCornerWidth;
    const int left = b.left() - kOut;
    const int top = b.top() - kOut;
    const int right = b.right() + kOut + 1;
    const int bottom = b.bottom() + kOut + 1;
    const QList<QRect> bars = {{left, top, kLen, kWide},
                               {left, top, kWide, kLen},
                               {right - kLen, top, kLen, kWide},
                               {right - kWide, top, kWide, kLen},
                               {left, bottom - kWide, kLen, kWide},
                               {left, bottom - kLen, kWide, kLen},
                               {right - kLen, bottom - kWide, kLen, kWide},
                               {right - kWide, bottom - kLen, kWide, kLen}};
    for (const QRect& bar : bars) {
        painter.fillRect(bar, Qt::white);
    }
    painter.restore();
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
        if (m_cropping) {
            paintCrop(painter, target);
        } else if (m_compare && !m_original.isNull()) {
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
