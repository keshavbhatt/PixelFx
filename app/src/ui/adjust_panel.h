#pragma once

#include "core/histogram.h"

#include <QWidget>

#include <array>
#include <pfx/settings.h>
#include <vector>

class QButtonGroup;
class QComboBox;
class QToolButton;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QSlider;

namespace pixelfx::ui {

class AdjustGroup;
class AdjustSlider;
class ColorWheel;
class CurveEditor;
class HistogramView;

/// The adjustments panel (mock editor.html, right side): groups in a photographer's order,
/// built from the library's operation registry (ADR-005). It edits a copy of the photo's
/// settings and reports every change; it holds no image logic.
class AdjustPanel : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AdjustPanel)

public:
    explicit AdjustPanel(QWidget* parent = nullptr);
    ~AdjustPanel() override;

    /// Shows `settings` without emitting anything (open, undo, redo).
    void setSettings(const pfx::Settings& settings);
    [[nodiscard]] const pfx::Settings& settings() const { return m_settings; }
    [[nodiscard]] AdjustSlider* slider(const QString& key) const;
    [[nodiscard]] CurveEditor* curveEditor() const { return m_curve; }
    /// Shows the curve of `key` (curves.rgb, curves.r, curves.g, curves.b) in the editor.
    void showCurve(const QString& key);
    /// Shows the colour mixer slider of `mode` (0 hue, 1 saturation, 2 luminance) and `band`
    /// (0 red to 7 magenta).
    void showMixer(int mode, int band);
    /// The colour mixer slider on show.
    [[nodiscard]] AdjustSlider* mixerSlider() const;
    /// The colour grading wheel of `zone` (0 shadows, 1 highlights).
    [[nodiscard]] ColorWheel* gradingWheel(int zone) const;
    void setHistogram(const core::Histogram& histogram);
    /// The exposure line under the histogram, and the camera and lens as its tooltip.
    void setPhotoDetails(const QString& exposure, const QString& cameraAndLens);
    /// Clears every setting (Ctrl+R) and reports it as one step.
    void resetAll();
    /// White balance shows "Auto" until a slider moves it.
    void markWhiteBalanceAuto();
    /// The picker button's state, without emitting.
    void setPicking(bool on);
    /// Paste is available when the clipboard holds PixelFx settings.
    void setPasteEnabled(bool enabled);

Q_SIGNALS:
    /// Every change, live while a slider moves.
    void settingsChanged(const pfx::Settings& settings);
    /// A change is complete (slider released, a reset): a history step named `label`.
    void editFinished(const QString& label);
    void autoRequested();
    void copyRequested();
    void whiteBalanceAutoRequested();
    /// The picker button: the next click on the photo picks a neutral point.
    void whiteBalancePickRequested(bool on);
    void pasteRequested();

private:
    void setValue(const QString& key, double value, bool finished);
    void resetGroup(AdjustGroup* group);
    void refreshGroups();
    void buildCurvesGroup(QWidget* content, QVBoxLayout* groups);
    void buildMixerGroup(QWidget* content, QVBoxLayout* groups);
    void buildGradingGroup(QWidget* content, QVBoxLayout* groups);
    QWidget* buildWheel(int zone, QWidget* parent);
    void wheelEdited(int zone, double hue, double amount, bool finished);
    void refreshWheels();
    [[nodiscard]] bool gradingWheelsChanged() const;
    QHBoxLayout* buildTopRow();
    QWidget* buildWhiteBalanceRow(QWidget* parent);
    void refreshWhiteBalance();
    void showMixerSlider();
    void curveEdited(const pfx::Curve& curve);

    pfx::Settings m_settings;
    HistogramView* m_histogram = nullptr;
    QPushButton* m_paste = nullptr;
    QComboBox* m_wbMode = nullptr;
    QToolButton* m_picker = nullptr;
    bool m_wbAuto = false;
    AdjustGroup* m_curvesGroup = nullptr;
    CurveEditor* m_curve = nullptr;
    QButtonGroup* m_curveChannels = nullptr;
    QString m_curveKey = QStringLiteral("curves.rgb");
    std::vector<AdjustGroup*> m_groups;
    std::vector<AdjustSlider*> m_sliders;
    std::vector<AdjustSlider*> m_mixerSliders;
    QButtonGroup* m_mixerModes = nullptr;
    QButtonGroup* m_mixerBands = nullptr;
    AdjustGroup* m_gradingGroup = nullptr;
    std::array<ColorWheel*, 2> m_wheels{};
    std::array<QLabel*, 2> m_wheelValues{};
};

/// One labelled slider for one numeric setting. Values are in the registry's units.
class AdjustSlider : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AdjustSlider)

public:
    AdjustSlider(const QString& key, const QString& label, QWidget* parent = nullptr);
    ~AdjustSlider() override;

    [[nodiscard]] const QString& key() const { return m_key; }
    [[nodiscard]] const QString& label() const { return m_label; }
    [[nodiscard]] double value() const;
    [[nodiscard]] double defaultValue() const { return m_default; }
    [[nodiscard]] bool isChanged() const;
    /// Shows `value` without emitting.
    void setValue(double value);
    /// The value as the panel shows it: "+12", "-0.35", "+1.5°".
    [[nodiscard]] QString valueText() const;
    /// Steps of 1/`steps` of a unit (the default is hundredths for small ranges, else whole).
    void setStepsPerUnit(double steps);

Q_SIGNALS:
    void moved(double value);
    void released(double value);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refresh();
    [[nodiscard]] int toTicks(double value) const;
    [[nodiscard]] double fromTicks(int ticks) const;

    QString m_key;
    QString m_label;
    double m_default = 0.0;
    double m_scale = 1.0;
    double m_minimum = 0.0;
    double m_maximum = 0.0;
    bool m_degrees = false;
    QSlider* m_slider = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_value = nullptr;
};

} // namespace pixelfx::ui
