#pragma once

#include "core/crop_geometry.h"
#include "ui/crop_overlay.h"

#include <QImage>
#include <QString>
#include <QWidget>

class QPushButton;
class QLabel;
class QMouseEvent;
class QPainter;
class QToolButton;

namespace pixelfx::ui {

/// A tool that takes the photo's left clicks and draws over it (Heal). `photo` is where the
/// photo is drawn, in widget coordinates; the middle button still pans.
class CanvasTool
{
public:
    CanvasTool() = default;
    virtual ~CanvasTool() = default;
    Q_DISABLE_COPY_MOVE(CanvasTool)

    virtual void paint(QPainter& painter, const QRectF& photo) = 0;
    /// True when the tool takes the press; it then gets the moves and the release.
    virtual bool press(const QMouseEvent& event, const QRectF& photo) = 0;
    /// A move while pressed, or hovering (for a brush outline).
    virtual void move(const QMouseEvent& event, const QRectF& photo) = 0;
    virtual void release(const QMouseEvent& event, const QRectF& photo) = 0;
    /// The pointer left the canvas.
    virtual void leave() {}
};

/// The line centred at the top of the canvas, on a card: what the canvas shows or what a drag
/// does ("Preview of Portrait 400"), with a muted note after it ("click the tile to apply").
void paintCanvasBanner(QPainter& painter, double canvasWidth, const QString& text, const QString& note);

/// The photo, fitted to the widget on the canvas colour (DESIGN section 2). Shows a status line
/// while opening and the reason when opening failed. In crop mode it draws the crop box over
/// the whole photo and reports where the box is dragged.
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
    /// Before and after next to each other instead of split by a divider.
    void setSideBySide(bool on);
    [[nodiscard]] bool isSideBySide() const { return m_sideBySide; }
    void setOriginal(const QImage& original);
    /// Divider position, 0 (all edited) to 1 (all original); starts in the middle.
    [[nodiscard]] double split() const { return m_split; }
    void setSplit(double split);
    void setStatus(const QString& text);
    /// A label in the photo's top left corner, outside before and after ("After" in the large
    /// preview); empty for none.
    void setTag(const QString& text);
    /// Work under way on the picture ("Loading", "Updating"), said beside the zoom bar once it
    /// has taken longer than kBusyDelay; empty when it is done.
    void setBusy(const QString& text);
    [[nodiscard]] bool showsBusy() const;
    static constexpr int kBusyDelay = 300;
    /// The photo shows a preset's preview, not the edit: a Preview tag and a line naming it.
    /// An empty name ends it.
    void setPreviewName(const QString& name);
    [[nodiscard]] const QString& previewName() const { return m_previewName; }
    /// A reason shown in place of the photo; `action` adds a button under it (errorActionTriggered).
    void setError(const QString& text, const QString& action = {});
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
    /// The colour around the photo (Settings, Canvas background); invalid for the theme's.
    void setBackground(const QColor& color);
    /// Clipped highlights in red and crushed shadows in blue, over the photo (J).
    void setClippingWarnings(bool on);
    [[nodiscard]] bool clippingWarnings() const { return m_clipping; }
    /// The overlay for `image`: red where a channel is at white, blue where all are at black.
    [[nodiscard]] static QImage clippingOverlay(const QImage& image);
    [[nodiscard]] bool needsDetail() const;

    /// The straighten ruler under the photo, shown in Crop mode.
    [[nodiscard]] class StraightenDial* dial() const { return m_dial; }
    /// The next drag draws a line instead of moving the crop box (Draw a level line).
    void startLevelLine();
    /// The next click on the photo reports where it landed (the white balance picker).
    void startPick();
    void cancelPick();
    [[nodiscard]] bool isPicking() const { return m_picking; }
    /// The tool that takes clicks on the photo (Heal), or none.
    void setTool(CanvasTool* tool);
    [[nodiscard]] CanvasTool* tool() const { return m_tool; }
    [[nodiscard]] bool isDrawingLevelLine() const { return m_levelLine; }

Q_SIGNALS:
    /// The crop box was dragged to `crop` (fractions of the shown photo).
    void cropEdited(const QRectF& crop);
    void zoomChanged();
    /// A level line was drawn; `degrees` is its tilt on screen, positive clockwise.
    void levelLineDrawn(double degrees);
    /// The busy chip came on show (after the delay) or went.
    void busyChanged(bool shown);
    /// The button under an error was pressed.
    void errorActionTriggered();
    /// A click while picking, as fractions of the photo.
    void pointPicked(QPointF fraction);
    void fullScreenRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    [[nodiscard]] QRect fitRect() const;
    [[nodiscard]] QSizeF zoomedSize() const;
    void clampCentre();
    void buildZoomBar();
    /// The chip beside the zoom bar that says the picture is being made (setBusy).
    void buildBusyChip();
    void updateZoomBar();
    class StraightenDial* m_dial = nullptr;
    bool m_levelLine = false;
    bool m_picking = false;
    QString m_previewName;
    QWidget* m_busy = nullptr;
    class QLabel* m_busyText = nullptr;
    class QTimer* m_busyTimer = nullptr;
    QColor m_background;
    bool m_clipping = false;
    QImage m_clipOverlay;
    qint64 m_clipKey = 0;
    bool m_drawingLine = false;
    QPointF m_lineStart;
    QPointF m_lineEnd;
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
    void paintPhoto(QPainter& painter);
    void paintCompare(class QPainter& painter, const QRect& target);
    void paintSideBySide(QPainter& painter);
    /// A "Before" or "After" label in a top corner of `photo`.
    void drawTag(QPainter& painter, const QRect& photo, const QString& text, bool right) const;
    QString m_tag;
    static constexpr double kKnobRadius = 17.0;
    void paintCrop(QPainter& painter, const QRect& target);
    void paintOverlay(QPainter& painter, const QRectF& box) const;
    bool m_cropping = false;
    CanvasTool* m_tool = nullptr;
    bool m_toolPressed = false;
    QRectF m_crop{0, 0, 1, 1};
    double m_cropRatio = 0.0;
    CropOverlay m_overlay = CropOverlay::Thirds;
    core::CropHandle m_handle = core::CropHandle::None;
    QPointF m_pressPos;
    QRectF m_pressBox;
    QImage m_original;
    bool m_compare = false;
    bool m_sideBySide = false;
    bool m_dragging = false;
    double m_split = 0.5;
    QImage m_image;
    QSize m_fullSize;
    QString m_status;
    QString m_error;
    QPushButton* m_errorAction = nullptr;
    void placeErrorAction();
};

} // namespace pixelfx::ui
