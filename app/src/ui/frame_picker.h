#pragma once

#include <QImage>
#include <QPointF>
#include <QWidget>

namespace pixelfx::ui {

/// What an export of another shape keeps (export-sizes.html): the photo dimmed, the part a
/// Crop to fill keeps bright inside a white frame that the mouse or the arrow keys move along
/// the free side; with Fit inside, the whole photo inside the frame with its border.
class FramePicker : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(FramePicker)

public:
    explicit FramePicker(QWidget* parent = nullptr);
    ~FramePicker() override;

    void setPicture(const QImage& photo);
    /// The frame's pixel size (its shape is what matters) and how the photo goes in it.
    void setFrame(const QSize& frame, bool fitInside);
    /// Where the kept part sits: 0 left or top, 0.5 the middle, 1 right or bottom.
    void setAnchor(QPointF anchor);
    [[nodiscard]] QPointF anchor() const { return m_anchor; }
    /// The kept part in the picker's own coordinates (for tests).
    [[nodiscard]] QRectF frameRect() const;

    [[nodiscard]] bool hasHeightForWidth() const override { return true; }
    [[nodiscard]] int heightForWidth(int width) const override;
    [[nodiscard]] QSize sizeHint() const override;
    /// As tall as the sizeHint: the picture is never squeezed under its buttons.
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    void anchorChanged(QPointF anchor);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    [[nodiscard]] QRectF photoRect() const;
    /// The spare length of the photo outside the frame, in widget pixels, per axis.
    [[nodiscard]] QSizeF spare() const;
    /// The way the frame can move: across a photo wider than the frame, or up and down one
    /// taller. From the shapes alone, so it holds before the picker is laid out.
    [[nodiscard]] Qt::Orientation freeDirection() const;
    void moveBy(QPointF pixels);

    QImage m_photo;
    QSize m_frame;
    bool m_fitInside = false;
    QPointF m_anchor{0.5, 0.5};
    QPointF m_dragFrom;
    bool m_dragging = false;
};

} // namespace pixelfx::ui
