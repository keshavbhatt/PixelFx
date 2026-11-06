#pragma once

#include "core/histogram.h"
#include "ui/slider_row.h"

#include <QWidget>

#include <array>
#include <functional>
#include <optional>
#include <pfx/mixer.h>
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
class CheckRow;
class ChoiceRow;
class ColorWheel;
class CurveEditor;
class GradingRangeButton;
struct ControlSpec;
struct GroupSpec;
class HistogramView;
class RegionCurveView;

/// The name the panel shows for the slider of `key` ("Clarity"); empty for keys without a slider.
[[nodiscard]] QString settingLabel(const QString& key);

/// The adjustments panel: groups in a photographer's order,
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
    /// A RAW file (editor-raw.html): the Camera profile is offered, Temperature reads in Kelvin
    /// around the as-shot white, and White balance offers the light sources.
    void setRaw(bool raw, int asShotKelvin);
    /// The photo's lens (`lens`, from its Exif) and the name of its Lensfun profile (`profile`,
    /// empty when none is found) for Use lens profile in Optics (editor-lens-profile.html). The
    /// rows show only when lens profiles are available.
    void setLens(const QString& lens, const QString& profile, bool profilesAvailable);
    [[nodiscard]] AdjustSlider* slider(const QString& key) const;
    [[nodiscard]] CurveEditor* curveEditor() const { return m_curve; }
    /// Shows the curve of `key` (curves.rgb, curves.r, curves.g, curves.b) in the editor.
    void showCurve(const QString& key);
    /// Shows the colour mixer slider of `mode` (0 hue, 1 saturation, 2 luminance) and `band`
    /// (0 red to 7 magenta).
    void showMixer(int mode, int band);
    /// The colour mixer slider on show.
    [[nodiscard]] AdjustSlider* mixerSlider() const;
    /// Shows colour grading range `range` (0 shadows, 1 midtones, 2 highlights, 3 global) on the
    /// large wheel.
    void showGradingRange(int range);
    [[nodiscard]] int gradingRange() const;
    /// The large colour grading wheel; it edits the range on show.
    [[nodiscard]] ColorWheel* gradingWheel() const { return m_wheel; }
    /// Points (false) or Regions (true) in the Curves group.
    void showCurveRegions(bool regions);
    [[nodiscard]] bool showsCurveRegions() const;
    [[nodiscard]] RegionCurveView* regionCurve() const { return m_regionCurve; }
    [[nodiscard]] AdjustGroup* group(const QString& id) const;
    [[nodiscard]] ChoiceRow* choiceRow(const QString& key) const;
    [[nodiscard]] CheckRow* checkRow(const QString& key) const;
    /// The target of the Color mixer (T): pick a colour on the photo, drag to change it.
    void setTargeting(bool on);
    [[nodiscard]] bool isTargeting() const;
    /// What a drag on the photo does now, for the line at the top and the hint.
    [[nodiscard]] QString targetBanner() const;
    [[nodiscard]] QString targetDragWords() const;
    /// The bands of a colour ("Blues and aquas"), or "No colour here" for a grey.
    [[nodiscard]] static QString targetName(const pfx::HueBands& bands);
    /// A press on the photo: shows the colour's main band and remembers where both bands start.
    void pickTarget(const pfx::HueBands& bands);
    /// A drag `steps` from the press: both bands move, the second by its share. `finished` is
    /// a history step.
    void dragTarget(double steps, bool finished);
    /// Color or Black & white (V): one step in the history.
    void setBlackAndWhite(bool on);
    [[nodiscard]] bool isBlackAndWhite() const;
    void setHistogram(const core::Histogram& histogram);
    /// The exposure line under the histogram, and the camera and lens as its tooltip.
    void setPhotoDetails(const QString& exposure, const QString& cameraAndLens);
    /// Clears every setting (Ctrl+R) and reports it as one step.
    void resetAll();
    /// The ids of the open groups ("Light", "Color grading"), for the next session.
    [[nodiscard]] QStringList openGroups() const;
    /// Opens the groups in `ids` and closes the others.
    void setOpenGroups(const QStringList& ids);
    /// Closes every group (the Collapse all button, a heading's menu).
    void collapseAll();
    void expandAll();
    /// Opens `group` and closes the others (Alt+click on a heading).
    void openOnly(AdjustGroup* group);
    /// One group open at a time: opening one closes the others (Lightroom's solo mode).
    void setOneGroupAtATime(bool on);
    [[nodiscard]] bool oneGroupAtATime() const { return m_solo; }
    /// The ids of the groups whose "More" is open, for the next session.
    [[nodiscard]] QStringList openMore() const;
    void setOpenMore(const QStringList& ids);
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
    /// "Crop and perspective" in the Geometry group.
    void cropRequested();
    /// The target button was toggled.
    void targetToggled(bool on);
    /// The mixer's mode or black and white changed: what a drag does changed.
    void targetModeChanged();
    /// One group open at a time was switched, from a heading's menu.
    void oneGroupAtATimeChanged(bool on);

private:
    struct Bound
    {
        QString key;
        QWidget* row;
    };
    void setValue(const QString& key, double value, bool finished);
    void setChoice(const QString& key, const std::string& value, const QString& step);
    void setFlag(const QString& key, bool on, const QString& step);
    void buildSpecGroup(const GroupSpec& spec, QWidget* content, QVBoxLayout* groups);
    void addSpecControl(AdjustGroup* group, const ControlSpec& control);
    void addDivider(AdjustGroup* group, const ControlSpec& control, bool lens);
    void refreshLens();
    QWidget* buildChoiceRow(const QString& key, const QString& shown, const QString& full, QWidget* parent);
    QWidget* buildCheckRow(const QString& key, const QString& shown, const QString& note, const QString& full,
                           QWidget* parent);
    QWidget* buildTreatmentRow(QWidget* parent);
    void refreshTreatment();
    void refreshRows();
    void resetGroup(AdjustGroup* group);
    void refreshGroups();
    void buildCurvesGroup(QWidget* content, QVBoxLayout* groups);
    AdjustSlider* addSlider(AdjustGroup* group, const QString& key, const QString& label, bool more = false,
                            double display = 1.0);
    void addGroup(AdjustGroup* group, QVBoxLayout* groups);
    void showHeadingMenu(AdjustGroup* group, const QPoint& where);
    void refreshCollapseAll();
    /// Shows the settings in the controls that are not sliders (curve, wheels).
    void refreshControls();
    void buildMixerGroup(QWidget* content, QVBoxLayout* groups);
    /// The eight swatches and the target button at their end.
    QWidget* buildMixerSwatches(QWidget* parent);
    void buildGeometryGroup(QWidget* content, QVBoxLayout* groups);
    /// A change to the turns, flips or straightening, through crop_geometry so the crop stays
    /// on the same part of the photo; a history step when `label` is given.
    void editGeometry(const std::function<void(pfx::Settings&)>& change, const QString& label);
    void buildGradingGroup(QWidget* content, QVBoxLayout* groups);
    QWidget* buildGradingRanges(QWidget* parent);
    void wheelEdited(double hue, double amount, bool finished);
    void refreshWheels();
    QWidget* buildCurvePoints(AdjustGroup* group);
    void buildCurveRegions(AdjustGroup* group);
    void splitMoved(int index, double value, bool finished);
    /// Shows Regions when the settings have only a region curve, Points when they have points.
    void followCurveMode();
    QHBoxLayout* buildTopRow();
    QWidget* buildWhiteBalanceRow(QWidget* parent);
    void refreshWhiteBalance();
    void showMixerSlider();
    void curveEdited(const pfx::Curve& curve);

    pfx::Settings m_settings;
    HistogramView* m_histogram = nullptr;
    QPushButton* m_paste = nullptr;
    QPushButton* m_collapseAll = nullptr;
    bool m_solo = false;
    QComboBox* m_wbMode = nullptr;
    bool m_raw = false;
    int m_asShotKelvin = 0;
    void fillWhiteBalanceModes();
    QToolButton* m_picker = nullptr;
    bool m_wbAuto = false;
    CurveEditor* m_curve = nullptr;
    QButtonGroup* m_curveChannels = nullptr;
    QString m_curveKey = QStringLiteral("curves.rgb");
    std::vector<AdjustGroup*> m_groups;
    std::vector<AdjustSlider*> m_sliders;
    std::vector<AdjustSlider*> m_mixerSliders;
    std::vector<AdjustSlider*> m_bwSliders;
    QToolButton* m_target = nullptr;
    QLabel* m_targetAlso = nullptr;
    struct TargetDrag
    {
        std::array<std::size_t, 2> bands{};
        double ratio = 0.0; // the second band moves by this share of the first's change
        std::array<double, 2> start{};
    };
    std::optional<TargetDrag> m_targetDrag;
    [[nodiscard]] QString targetKey(std::size_t band) const;
    /// "hue", "saturation" or "luminance": the mixer's mode on show.
    [[nodiscard]] QString mixerModeWord() const;
    QWidget* m_mixerModeRow = nullptr;
    QButtonGroup* m_mixerModes = nullptr;
    QButtonGroup* m_mixerBands = nullptr;
    AdjustGroup* m_mixerGroup = nullptr;
    QButtonGroup* m_treatment = nullptr;
    std::vector<Bound> m_choices;
    std::vector<Bound> m_checks;
    std::vector<QWidget*> m_lensRows;
    std::vector<AdjustSlider*> m_lensSliders;
    QString m_lens;
    QString m_lensProfile;
    bool m_lensProfiles = false;
    QButtonGroup* m_gradingRanges = nullptr;
    std::array<GradingRangeButton*, 4> m_rangeButtons{};
    std::array<AdjustSlider*, 4> m_rangeLuminance{};
    ColorWheel* m_wheel = nullptr;
    QLabel* m_wheelValues = nullptr;
    QButtonGroup* m_curveModes = nullptr;
    std::vector<QWidget*> m_pointsWidgets;
    std::vector<QWidget*> m_regionsWidgets;
    RegionCurveView* m_regionCurve = nullptr;
};

/// The slider of one numeric setting, in the registry's units (stops, degrees) and range.
class AdjustSlider : public SliderRow
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AdjustSlider)

public:
    /// `display` scales the value shown (a share of the long edge as pixels of a 6000 px photo,
    /// the way Lightroom writes a radius); such values step in tenths of the shown unit.
    AdjustSlider(const QString& key, const QString& label, QWidget* parent = nullptr, double display = 1.0);
    ~AdjustSlider() override;

    [[nodiscard]] const QString& key() const { return m_key; }
    [[nodiscard]] double value() const;
    [[nodiscard]] double defaultValue() const { return m_default; }
    /// Shows `value` without emitting.
    void setValue(double value);
    /// The value as the panel shows it: "+12", "-0.35", "+1.5°".
    [[nodiscard]] QString valueText() const { return text(); }
    /// Steps of 1/`steps` of a unit (the default is hundredths for small ranges, else whole).
    void setStepsPerUnit(double steps);

Q_SIGNALS:
    void moved(double value);
    void released(double value);

private:
    [[nodiscard]] int toTicks(double value) const;
    [[nodiscard]] double fromTicks(int ticks) const;
    [[nodiscard]] QString format(int ticks) const;

public:
    /// Reads in Kelvin around `asShot` (a RAW's white balance); 0 goes back to the plain scale.
    void setKelvin(int asShot);

private:
    QString m_key;
    int m_kelvin = 0;
    double m_default = 0.0;
    double m_scale = 1.0;   // ticks per shown unit
    double m_display = 1.0; // shown unit per setting unit
    double m_minimum = 0.0;
    double m_maximum = 0.0;
    bool m_degrees = false;
};

} // namespace pixelfx::ui
