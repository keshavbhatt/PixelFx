#include "ui/frame_picker.h"

#include "ui/style.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace pixelfx::ui {

namespace {

constexpr float kDim = 0.55F;   // the black laid over what is cropped away
constexpr double kStep = 0.05;  // an arrow key's move, of the free range
constexpr int kMaxHeight = 170; // the picture's height at most, so the sheet stays short

} // namespace

FramePicker::FramePicker(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setAccessibleName(tr("The part of the photo that is kept"));
    setAccessibleDescription(tr("Drag the frame, or use the arrow keys, to choose what stays"));
}

FramePicker::~FramePicker() = default;

void FramePicker::setPicture(const QImage& photo)
{
    m_photo = photo;
    updateGeometry();
    update();
}

void FramePicker::setFrame(const QSize& frame, bool fitInside)
{
    m_frame = frame;
    m_fitInside = fitInside;
    setCursor(fitInside ? Qt::ArrowCursor : Qt::OpenHandCursor);
    update();
}

void FramePicker::setAnchor(QPointF anchor)
{
    anchor = QPointF(std::clamp(anchor.x(), 0.0, 1.0), std::clamp(anchor.y(), 0.0, 1.0));
    if (anchor != m_anchor) {
        m_anchor = anchor;
        update();
        Q_EMIT anchorChanged(anchor);
    }
}

int FramePicker::heightForWidth(int width) const
{
    const int natural =
        (m_photo.isNull() || m_photo.width() == 0)
            ? width * 2 / 3
            : static_cast<int>(std::lround(static_cast<double>(width) * m_photo.height() / m_photo.width()));
    return std::min(natural, kMaxHeight);
}

QSize FramePicker::sizeHint() const
{
    constexpr int kWidth = 260;
    return {kWidth, heightForWidth(kWidth)};
}

QRectF FramePicker::photoRect() const
{
    // The photo's shape, as large as the picker allows, in the middle.
    const QSizeF area(width(), heightForWidth(width()));
    const QSizeF shape = m_photo.isNull() ? area : QSizeF(m_photo.size()).scaled(area, Qt::KeepAspectRatio);
    return {(area.width() - shape.width()) / 2, (area.height() - shape.height()) / 2, shape.width(),
            shape.height()};
}

QSizeF FramePicker::spare() const
{
    const QRectF photo = photoRect();
    if (m_frame.isEmpty() || photo.isEmpty()) {
        return {};
    }
    // The frame covers the photo in one direction and leaves the rest spare in the other.
    const double frameRatio = static_cast<double>(m_frame.width()) / m_frame.height();
    const double photoRatio = photo.width() / photo.height();
    if (frameRatio < photoRatio) {
        return {photo.width() - (photo.height() * frameRatio), 0.0};
    }
    return {0.0, photo.height() - (photo.width() / frameRatio)};
}

QRectF FramePicker::frameRect() const
{
    const QRectF photo = photoRect();
    const QSizeF free = spare();
    return {photo.left() + (free.width() * m_anchor.x()), photo.top() + (free.height() * m_anchor.y()),
            photo.width() - free.width(), photo.height() - free.height()};
}

void FramePicker::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF photo = photoRect();
    QPainterPath round;
    round.addRoundedRect(photo, 6, 6);
    p.setClipPath(round);
    if (m_fitInside && !m_frame.isEmpty()) {
        // The result: the frame's shape with the whole photo inside it on its border.
        const double frameRatio = static_cast<double>(m_frame.width()) / m_frame.height();
        QSizeF box = photo.size();
        box = frameRatio < photo.width() / photo.height()
                  ? QSizeF(photo.height() * frameRatio, photo.height())
                  : QSizeF(photo.width(), photo.width() / frameRatio);
        const QRectF frame(photo.center() - QPointF(box.width() / 2, box.height() / 2), box);
        p.fillRect(photo, tokens().canvas);
        p.fillRect(frame, Qt::white);
        const QSizeF inner = photo.size().scaled(frame.size(), Qt::KeepAspectRatio);
        p.drawImage(QRectF(frame.center() - QPointF(inner.width() / 2, inner.height() / 2), inner), m_photo);
        p.setClipping(false);
        p.setPen(QPen(Qt::white, 2.0));
        p.drawRect(frame.adjusted(1, 1, -1, -1));
        return;
    }
    p.drawImage(photo, m_photo);
    const QRectF frame = frameRect();
    if (m_frame.isEmpty()) {
        return;
    }
    QPainterPath outside;
    outside.addRect(photo);
    outside.addRect(frame);
    p.fillPath(outside, QColor::fromRgbF(0, 0, 0, kDim));
    p.setClipping(false);
    p.setPen(QPen(Qt::white, 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(frame.adjusted(1, 1, -1, -1));
    if (hasFocus()) {
        p.setPen(QPen(tokens().accent, 2.0));
        p.drawRoundedRect(photo.adjusted(1, 1, -1, -1), 6, 6);
    }
}

void FramePicker::moveBy(QPointF pixels)
{
    const QSizeF free = spare();
    setAnchor(QPointF(free.width() > 0 ? m_anchor.x() + (pixels.x() / free.width()) : m_anchor.x(),
                      free.height() > 0 ? m_anchor.y() + (pixels.y() / free.height()) : m_anchor.y()));
}

void FramePicker::mousePressEvent(QMouseEvent* event)
{
    if (!m_fitInside && event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragFrom = event->position();
        // A press outside the frame moves its middle there.
        if (!frameRect().contains(event->position())) {
            moveBy(event->position() - frameRect().center());
        }
        setCursor(Qt::ClosedHandCursor);
    }
}

void FramePicker::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        moveBy(event->position() - m_dragFrom);
        m_dragFrom = event->position();
    }
}

void FramePicker::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    m_dragging = false;
    if (!m_fitInside) {
        setCursor(Qt::OpenHandCursor);
    }
}

void FramePicker::keyPressEvent(QKeyEvent* event)
{
    const QSizeF free = spare();
    const QPointF step(free.width() * kStep, free.height() * kStep);
    switch (event->key()) {
    case Qt::Key_Left:
        moveBy(QPointF(-step.x(), 0));
        return;
    case Qt::Key_Right:
        moveBy(QPointF(step.x(), 0));
        return;
    case Qt::Key_Up:
        moveBy(QPointF(0, -step.y()));
        return;
    case Qt::Key_Down:
        moveBy(QPointF(0, step.y()));
        return;
    case Qt::Key_Home:
        setAnchor({0.5, 0.5});
        return;
    default:
        QWidget::keyPressEvent(event);
    }
}

} // namespace pixelfx::ui
