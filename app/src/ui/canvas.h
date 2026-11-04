#pragma once

#include "core/crop_geometry.h"
#include "ui/crop_overlay.h"

#include <QImage>
#include <QString>
#include <QWidget>

class QLabel;
class QToolButton;

namespace pixelfx::ui {

/// The photo, fitted to the widget on the canvas colour (DESIGN section 2). Shows a status line
/// while opening and the reason when opening failed. In crop mode it draws the crop box over
/// the whole photo (mock crop.html) and reports where the box is dragged.
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

    void setCropMode(bool on);
    [[nodiscard]] bool isCropping() const { return m_cropping; }
    /// The crop box as fractions of the shown photo.
    void setCrop(const QRectF& crop);
    [[nodiscard]] const QRectF& crop() const { return m_crop; }
    /// Width over height the box keeps while dragged; 0 is free (Shift keeps the current one).
    void setCropRatio(double ratio);
    void setCropOverlay(CropOverlay overlay);
    /// Where the crop box is in widget coordinates.
    [[nodiscard]] QRectF cropBox() const;
    /// What a press at `pos` would drag.
    [[nodiscard]] core::CropHandle cropHandleAt(QPointF pos) const;

    /// Zoom: Fit (the default) shows the whole photo; otherwise the scale is screen pixels per
    /// photo pixel at its full size (1 is 100%), kept around `anchor` (widget coordinates; the
    /// middle when null). Crop mode always fits.
    void setZoom(double scale, QPointF anchor = {});
    void zoomToFit();
    void zoomIn();
    void zoomOut();
    [[nodiscard]] bool isFit() const { return m_zoom <= 0.0; }
    /// The scale on screen now, Fit included.
    [[nodiscard]] double scale() const;
    /// "Fit" or "45%", as the zoom bar shows it.
    [[nodiscard]] QString zoomText() const;
    /// The part of the photo on screen, as fractions of it.
    [[nodiscard]] QRectF visibleFraction() const;
    /// A sharper picture of the whole photo (the full-size render), drawn instead of the
    /// preview while zoomed in past the preview's own resolution. Null to drop it.
    void setDetail(const QImage& detail);
    [[nodiscard]] bool needsDetail() const;

Q_SIGNALS:
    /// The crop box was dragged to `crop` (fractions of the shown photo).
    void cropEdited(const QRectF& crop);
    void zoomChanged();
    void fullScreenRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    [[nodiscard]] QRect fitRect() const;
    [[nodiscard]] QSizeF zoomedSize() const;
    void clampCentre();
    void buildZoomBar();
    void updateZoomBar();
    double m_zoom = 0.0;        // 0: Fit
    QPointF m_centre{0.5, 0.5}; // the photo point at the middle of the widget, as fractions
    bool m_panning = false;
    QPointF m_panStart;
    QPointF m_centreStart;
    QImage m_detail;
    QWidget* m_zoomBar = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QToolButton* m_zoomOut = nullptr;
    QToolButton* m_zoomIn = nullptr;
    QToolButton* m_actual = nullptr;
    void paintCompare(class QPainter& painter, const QRect& target);
    void paintCrop(QPainter& painter, const QRect& target);
    void paintOverlay(QPainter& painter, const QRectF& box) const;
    bool m_cropping = false;
    QRectF m_crop{0, 0, 1, 1};
    double m_cropRatio = 0.0;
    CropOverlay m_overlay = CropOverlay::Thirds;
    core::CropHandle m_handle = core::CropHandle::None;
    QPointF m_pressPos;
    QRectF m_pressBox;
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
