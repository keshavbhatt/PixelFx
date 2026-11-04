#pragma once

#include "ui/crop_overlay.h"

#include <QRectF>
#include <QSize>
#include <QWidget>

#include <pfx/settings.h>

class QAbstractButton;
class QButtonGroup;
class QLayout;
class QPushButton;

namespace pixelfx::ui {

class AdjustSlider;

/// The Crop mode panel (mock crop.html, right side): aspect ratio, orientation, straighten,
/// quarter turns, flips and the overlay. It edits a copy of the photo's settings and reports
/// every change live; Done and Cancel end the mode. It holds no image logic.
class CropPanel : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CropPanel)

public:
    explicit CropPanel(QWidget* parent = nullptr);
    ~CropPanel() override;

    /// Starts editing `settings` of a photo whose upright size is `photo`. The ratio starts
    /// free.
    void begin(const pfx::Settings& settings, const QSize& photo);
    [[nodiscard]] const pfx::Settings& settings() const { return m_settings; }
    /// The crop box was moved on the canvas (fractions of the straightened photo).
    void setCrop(const QRectF& crop);
    /// Width over height in pixels; 0 when free.
    [[nodiscard]] double ratio() const { return m_ratio; }
    /// Picks a ratio chip by its label ("Free", "Original", "1:1", "4:5", "5:7", "3:2", "16:9",
    /// "9:16") and fits the largest crop of it.
    void chooseRatio(const QString& name);
    [[nodiscard]] CropOverlay overlay() const { return m_overlay; }
    /// Landscape and portrait swap (X).
    void swapOrientation();
    void turn(bool clockwise);
    void flip(Qt::Orientation orientation);
    void setAngle(double degrees);
    /// Clears turn, flips, straightening and crop; the other settings stay.
    void reset();

Q_SIGNALS:
    void settingsChanged(const pfx::Settings& settings);
    void ratioChanged(double ratio);
    void overlayChanged(pixelfx::ui::CropOverlay overlay);
    /// Done (`apply` true) or Cancel.
    void finished(bool apply);

private:
    QWidget* buildRatios();
    QWidget* buildOrientation();
    QLayout* buildStraighten();
    QWidget* buildOverlays();
    QLayout* buildFoot();
    void changed();
    void setRatio(double ratio);
    void refreshControls();
    [[nodiscard]] QSize uncropped() const;

    pfx::Settings m_settings;
    QSize m_photo;
    double m_ratio = 0.0;
    CropOverlay m_overlay = CropOverlay::Thirds;
    QButtonGroup* m_ratios = nullptr;
    QButtonGroup* m_orientation = nullptr;
    QButtonGroup* m_overlays = nullptr;
    AdjustSlider* m_angle = nullptr;
};

} // namespace pixelfx::ui
