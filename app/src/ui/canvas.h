#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

namespace pixelfx::ui {

/// The photo, fitted to the widget on the canvas colour (DESIGN section 2). Shows a status line
/// while opening and the reason when opening failed. Zoom and pan arrive with M3.
class Canvas : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Canvas)

public:
    explicit Canvas(QWidget* parent = nullptr);
    ~Canvas() override;

    /// `fullSize` is the photo's real size: the fit view never enlarges past 100% of it.
    void setImage(const QImage& image, const QSize& fullSize);
    /// Swaps the shown pixels (same photo: original or edited) without touching the full size.
    void showImage(const QImage& image);
    /// Before and after: the original left of a draggable divider, the edit to the right.
    void setCompare(bool on);
    [[nodiscard]] bool isComparing() const { return m_compare; }
    void setOriginal(const QImage& original);
    /// Divider position, 0 (all edited) to 1 (all original); starts in the middle.
    [[nodiscard]] double split() const { return m_split; }
    void setSplit(double split);
    void setStatus(const QString& text);
    void setError(const QString& text);
    [[nodiscard]] const QImage& image() const { return m_image; }
    /// Where the photo is drawn, in widget coordinates.
    [[nodiscard]] QRect photoRect() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void paintCompare(class QPainter& painter, const QRect& target);
    QImage m_original;
    bool m_compare = false;
    bool m_dragging = false;
    double m_split = 0.5;
    QImage m_image;
    QSize m_fullSize;
    QString m_status;
    QString m_error;
};

} // namespace pixelfx::ui
