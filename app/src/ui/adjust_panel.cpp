#include "ui/adjust_panel.h"

#include "core/crop_geometry.h"
#include "ui/adjust_group.h"
#include "ui/color_swatch.h"
#include "ui/color_wheel.h"
#include "ui/curve_editor.h"
#include "ui/histogram_view.h"
#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/region_curve.h"
#include "ui/setting_rows.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <pfx/registry.h>
#include <string_view>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

/// What a row of a spec group is.
enum class ControlKind
{
    Slider,
    Choice,       // a named choice of the registry (profile, vignette style)
    Check,        // an on or off setting with a line saying what it does
    Treatment,    // Color or Black & white
    WhiteBalance, // As shot, Auto or Custom, and the picker
};

/// One row of a group built from specs. `caption` starts a sub-heading before the row; `more`
/// puts it under the group's "More"; `fullName` is what history and screen readers say when the
/// shown label is short ("Detail" under "Sharpening"); `display` scales the shown value.
struct ControlSpec
{
    const char* key;
    const char* label = nullptr;
    const char* track = "";
    const char* caption = nullptr;
    bool more = false;
    const char* fullName = nullptr;
    ControlKind kind = ControlKind::Slider;
    double display = 1.0;
    const char* note = nullptr;
};

struct GroupSpec
{
    const char* name;
    const char* glyph;
    std::vector<ControlSpec> controls;
};

namespace {

// A radius is a share of the long edge; it reads as the pixels of a 6000 px photo, the size
// Lightroom's radius refers to (the importer uses the same).
constexpr double kRadiusPixels = 6000.0;

// The panel's order and wording (DESIGN section 3, editor-develop.html), Lightroom's words for
// every setting. Ranges, defaults and units come from the registry; names live here (ADR-005).
GroupSpec lightGroup()
{
    return {QT_TRANSLATE_NOOP("AdjustPanel", "Light"),
            "sun",
            {{.key = "light.exposure", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Exposure")},
             {.key = "light.contrast", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Contrast")},
             {.key = "light.highlights", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Highlights")},
             {.key = "light.shadows", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Shadows")},
             {.key = "light.whites", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Whites")},
             {.key = "light.blacks", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Blacks")}}};
}

GroupSpec colorGroup()
{
    using K = ControlKind;
    return {
        QT_TRANSLATE_NOOP("AdjustPanel", "Color"),
        "droplet",
        {{.key = "bw.enabled", .kind = K::Treatment},
         {.key = "color.profile", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Profile"), .kind = K::Choice},
         {.key = "color.wb.mode", .kind = K::WhiteBalance},
         {.key = "color.wb.temperature",
          .label = QT_TRANSLATE_NOOP("AdjustPanel", "Temperature"),
          .track = "temperatureTrack"},
         {.key = "color.wb.tint", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Tint"), .track = "tintTrack"},
         {.key = "color.vibrance", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Vibrance")},
         {.key = "color.saturation",
          .label = QT_TRANSLATE_NOOP("AdjustPanel", "Saturation"),
          .track = "saturationTrack"}}};
}

GroupSpec detailGroup()
{
    return {
        QT_TRANSLATE_NOOP("AdjustPanel", "Detail"),
        "detail",
        {{.key = "detail.texture", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Texture")},
         {.key = "detail.clarity", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Clarity")},
         {.key = "detail.dehaze", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Dehaze")},
         {.key = "detail.sharpen.amount", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Sharpen")},
         {.key = "detail.denoise.luminance", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Noise reduction")},
         {.key = "detail.denoise.color", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Color noise")},
         {.key = "detail.sharpen.radius",
          .label = QT_TRANSLATE_NOOP("AdjustPanel", "Radius"),
          .caption = QT_TRANSLATE_NOOP("AdjustPanel", "Sharpening"),
          .more = true,
          .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Sharpening radius"),
          .display = kRadiusPixels},
         {.key = "detail.sharpen.detail",
          .label = QT_TRANSLATE_NOOP("AdjustPanel", "Detail"),
          .more = true,
          .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Sharpening detail")},
         {.key = "detail.sharpen.masking",
          .label = QT_TRANSLATE_NOOP("AdjustPanel", "Masking"),
          .more = true,
          .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Sharpening masking")},
         {.key = "detail.denoise.luminanceDetail",
          .label = QT_TRANSLATE_NOOP("AdjustPanel", "Detail"),
          .caption = QT_TRANSLATE_NOOP("AdjustPanel", "Noise reduction"),
          .more = true,
          .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Noise reduction detail")},
         {.key = "detail.denoise.luminanceContrast",
          .label = QT_TRANSLATE_NOOP("AdjustPanel", "Contrast"),
          .more = true,
          .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Noise reduction contrast")},
         {.key = "detail.denoise.colorDetail",
          .label = QT_TRANSLATE_NOOP("AdjustPanel", "Detail"),
          .caption = QT_TRANSLATE_NOOP("AdjustPanel", "Color noise"),
          .more = true,
          .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Color noise detail")},
         {.key = "detail.denoise.colorSmoothness",
          .label = QT_TRANSLATE_NOOP("AdjustPanel", "Smoothness"),
          .more = true,
          .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Color noise smoothness")}}};
}

GroupSpec effectsGroup()
{
    using K = ControlKind;
    return {QT_TRANSLATE_NOOP("AdjustPanel", "Effects"),
            "sparkles",
            {{.key = "effects.vignette.amount", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Vignette")},
             {.key = "effects.grain.amount", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Grain")},
             {.key = "effects.fade", .label = QT_TRANSLATE_NOOP("AdjustPanel", "Fade")},
             {.key = "effects.vignette.style",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Style"),
              .caption = QT_TRANSLATE_NOOP("AdjustPanel", "Vignette"),
              .more = true,
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Vignette style"),
              .kind = K::Choice},
             {.key = "effects.vignette.midpoint",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Midpoint"),
              .more = true,
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Vignette midpoint")},
             {.key = "effects.vignette.roundness",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Roundness"),
              .more = true,
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Vignette roundness")},
             {.key = "effects.vignette.feather",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Feather"),
              .more = true,
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Vignette feather")},
             {.key = "effects.vignette.highlights",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Highlights"),
              .more = true,
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Vignette highlights")},
             {.key = "effects.grain.size",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Size"),
              .caption = QT_TRANSLATE_NOOP("AdjustPanel", "Grain"),
              .more = true,
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Grain size")},
             {.key = "effects.grain.roughness",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Roughness"),
              .more = true,
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Grain roughness")}}};
}

GroupSpec opticsGroup()
{
    using K = ControlKind;
    return {QT_TRANSLATE_NOOP("AdjustPanel", "Optics"),
            "focus",
            {{.key = "detail.ca.auto",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Remove chromatic aberration"),
              .kind = K::Check,
              .note = QT_TRANSLATE_NOOP(
                  "AdjustPanel", "Colour fringes along edges far from the middle, measured on the photo")},
             {.key = "detail.defringe.purple.amount",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Purple"),
              .caption = QT_TRANSLATE_NOOP("AdjustPanel", "Defringe"),
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Defringe purple")},
             {.key = "detail.defringe.green.amount",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Green"),
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Defringe green")},
             {.key = "detail.defringe.purple.hueLow",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "From"),
              .caption = QT_TRANSLATE_NOOP("AdjustPanel", "Purple hues"),
              .more = true,
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Purple hues from")},
             {.key = "detail.defringe.purple.hueHigh",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "To"),
              .more = true,
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Purple hues to")},
             {.key = "detail.defringe.green.hueLow",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "From"),
              .caption = QT_TRANSLATE_NOOP("AdjustPanel", "Green hues"),
              .more = true,
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Green hues from")},
             {.key = "detail.defringe.green.hueHigh",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "To"),
              .more = true,
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Green hues to")}}};
}

GroupSpec calibrationGroup()
{
    return {QT_TRANSLATE_NOOP("AdjustPanel", "Calibration"),
            "circle-dot",
            {{.key = "calibration.shadows.tint",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Shadows tint"),
              .track = "tintTrack"},
             {.key = "calibration.red.hue",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Hue"),
              .track = "redHueTrack",
              .caption = QT_TRANSLATE_NOOP("AdjustPanel", "Red primary"),
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Red primary hue")},
             {.key = "calibration.red.saturation",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Saturation"),
              .track = "redSaturationTrack",
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Red primary saturation")},
             {.key = "calibration.green.hue",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Hue"),
              .track = "greenHueTrack",
              .caption = QT_TRANSLATE_NOOP("AdjustPanel", "Green primary"),
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Green primary hue")},
             {.key = "calibration.green.saturation",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Saturation"),
              .track = "greenSaturationTrack",
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Green primary saturation")},
             {.key = "calibration.blue.hue",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Hue"),
              .track = "blueHueTrack",
              .caption = QT_TRANSLATE_NOOP("AdjustPanel", "Blue primary"),
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Blue primary hue")},
             {.key = "calibration.blue.saturation",
              .label = QT_TRANSLATE_NOOP("AdjustPanel", "Saturation"),
              .track = "blueSaturationTrack",
              .fullName = QT_TRANSLATE_NOOP("AdjustPanel", "Blue primary saturation")}}};
}

const std::vector<GroupSpec>& groupSpecs()
{
    static const std::vector<GroupSpec> specs = {lightGroup(),   colorGroup(),  detailGroup(),
                                                 effectsGroup(), opticsGroup(), calibrationGroup()};
    return specs;
}

/// What a choice's values read as, in the registry's order.
QStringList choiceTexts(std::string_view key)
{
    if (key == "color.profile") {
        return {AdjustPanel::tr("Standard"), AdjustPanel::tr("Vivid"), AdjustPanel::tr("Camera")};
    }
    if (key == "effects.vignette.style") {
        return {AdjustPanel::tr("Highlight priority"), AdjustPanel::tr("Color priority"),
                AdjustPanel::tr("Paint overlay")};
    }
    return {};
}

} // namespace

QString settingLabel(const QString& key)
{
    for (const GroupSpec& g : groupSpecs()) {
        for (const ControlSpec& c : g.controls) {
            if (c.label != nullptr && key == QLatin1StringView(c.key)) {
                return AdjustPanel::tr(c.fullName != nullptr ? c.fullName : c.label);
            }
        }
    }
    return {};
}

// ---------------------------------------------------------------------------------------------

AdjustSlider::AdjustSlider(const QString& key, const QString& label, QWidget* parent, double display)
    : SliderRow(label, parent)
    , m_key(key)
    , m_display(display)
{
    const pfx::ParamSpec* spec = pfx::registry().param(key.toStdString());
    Q_ASSERT(spec != nullptr);
    m_default = spec->defaultValue;
    m_minimum = spec->minimum;
    m_maximum = spec->maximum;
    m_degrees = spec->unit == "degrees";
    setFormat([this](int ticks) { return format(ticks); });
    // Small ranges (exposure in stops) get hundredths; the rest whole numbers.
    double steps = (spec->maximum - spec->minimum) <= 10.0 ? 100.0 : 1.0;
    if (display != 1.0) {
        steps = 10.0;
    }
    setStepsPerUnit(steps);
    setValue(m_default);
    connect(this, &SliderRow::ticksMoved, this, [this](int ticks) { Q_EMIT moved(fromTicks(ticks)); });
    connect(this, &SliderRow::ticksReleased, this, [this](int ticks) { Q_EMIT released(fromTicks(ticks)); });
}

AdjustSlider::~AdjustSlider() = default;

void AdjustSlider::setStepsPerUnit(double steps)
{
    const double current = value();
    m_scale = steps;
    setTicksPerUnit(steps);
    setRange(toTicks(m_minimum), toTicks(m_maximum), toTicks(m_default));
    // Exposure's keys step a twentieth of a stop, a stop with Page Up and Down; whole numbers 1 and 10.
    slider()->setSingleStep(m_scale >= 100.0 ? 5 : 1);
    slider()->setPageStep(static_cast<int>(m_scale > 1.0 ? m_scale : 10));
    setTicks(toTicks(current));
}

int AdjustSlider::toTicks(double value) const
{
    return static_cast<int>(std::lround(value * m_display * m_scale));
}

double AdjustSlider::fromTicks(int ticks) const
{
    return static_cast<double>(ticks) / m_scale / m_display;
}

double AdjustSlider::value() const
{
    return fromTicks(ticks());
}

void AdjustSlider::setValue(double value)
{
    setTicks(toTicks(value));
}

void AdjustSlider::setKelvin(int asShot)
{
    m_kelvin = asShot;
    // Typed Kelvin becomes the relative temperature the engine keeps.
    setTypedUnit(asShot > 0 ? std::function<double(double)>([asShot](double kelvin) {
        return kelvin > 0.0 ? 100.0 * std::log2(kelvin / asShot) : 0.0;
    })
                            : std::function<double(double)>());
    setTicks(ticks()); // the value reads again in the new unit
}

QString AdjustSlider::format(int ticks) const
{
    if (m_kelvin > 0) {
        // The light the photo is treated as lit by, around the as-shot white, to 50 K.
        const double kelvin = m_kelvin * std::exp2(static_cast<double>(ticks) / m_scale / 100.0);
        //: A RAW's colour temperature: "5150 K".
        return AdjustPanel::tr("%1 K").arg(std::lround(kelvin / 50.0) * 50);
    }
    const double v = static_cast<double>(ticks) / m_scale; // in the shown unit
    int decimals = 0;
    if (m_scale >= 100.0) {
        decimals = 2;
    } else if (m_scale > 1.0) {
        decimals = 1;
    }
    QString number = decimals > 0 ? QString::number(v, 'f', decimals) : QString::number(std::lround(v));
    if (m_degrees) {
        number += u'\u00B0';
    }
    // A sign only where the value can also go below zero ("+12", but "30" for Sharpen).
    return v > 0.0 && m_minimum < 0.0 ? u"+"_s + number : number;
}

// ---------------------------------------------------------------------------------------------

QHBoxLayout* AdjustPanel::buildTopRow()
{
    auto* resetAll = new IconTextButton(icons::themed(u"reset"_s, tokens().text), QString(), this);
    resetAll->setObjectName(u"small"_s);
    resetAll->setToolTip(tr("Reset all adjustments (Ctrl+R)"));
    resetAll->setAccessibleName(tr("Reset all"));
    auto* copy = new IconTextButton(icons::themed(u"copy"_s, tokens().text), tr("Copy"), this);
    copy->setObjectName(u"small"_s);
    copy->setToolTip(tr("Copy these settings, crop excepted (Ctrl+Shift+C)"));
    connect(copy, &QPushButton::clicked, this, &AdjustPanel::copyRequested);
    m_paste = new QPushButton(tr("Paste"), this);
    m_paste->setObjectName(u"small"_s);
    m_paste->setToolTip(tr("Paste copied settings onto this photo (Ctrl+Shift+V)"));
    m_paste->setEnabled(false);
    connect(m_paste, &QPushButton::clicked, this, &AdjustPanel::pasteRequested);
    connect(resetAll, &QPushButton::clicked, this, &AdjustPanel::resetAll);
    auto* autoButton = new IconTextButton(icons::themed(u"auto"_s, tokens().text), tr("Auto"), this);
    autoButton->setObjectName(u"small"_s);
    autoButton->setToolTip(tr("Set light and color from the photo (Ctrl+Shift+A)"));
    connect(autoButton, &QPushButton::clicked, this, &AdjustPanel::autoRequested);
    auto* top = new QHBoxLayout;
    top->setContentsMargins(kPanelPadding, 6, kPanelPadding, 8);
    top->setSpacing(kGapRelated);
    top->addWidget(autoButton, 1);
    top->addWidget(copy);
    top->addWidget(m_paste);
    top->addWidget(resetAll);
    m_collapseAll = new IconTextButton(icons::themed(u"chevrons-down-up"_s, tokens().text), QString(), this);
    m_collapseAll->setObjectName(u"small"_s);
    m_collapseAll->setToolTip(tr("Collapse all groups"));
    m_collapseAll->setAccessibleName(tr("Collapse all groups"));
    connect(m_collapseAll, &QPushButton::clicked, this, &AdjustPanel::collapseAll);
    top->addWidget(m_collapseAll);
    return top;
}

AdjustPanel::AdjustPanel(QWidget* parent)
    : QWidget(parent)
    , m_histogram(new HistogramView(this))
{
    makeSidePanel(this, Qt::LeftEdge);
    QHBoxLayout* top = buildTopRow();
    auto* histogramBox = new QVBoxLayout;
    histogramBox->setContentsMargins(kPanelPadding, 10, kPanelPadding, 0);
    histogramBox->addWidget(m_histogram);

    auto* content = new QWidget(this);
    content->setObjectName(u"panelContent"_s);
    auto* groups = new QVBoxLayout(content);
    groups->setContentsMargins(0, 0, 0, 0);
    groups->setSpacing(0);
    for (const GroupSpec& g : groupSpecs()) {
        buildSpecGroup(g, content, groups);
        if (std::string_view(g.name) == "Color") {
            buildMixerGroup(content, groups);
            buildGradingGroup(content, groups);
            buildCurvesGroup(content, groups);
        }
    }
    buildGeometryGroup(content, groups);
    groups->addStretch();

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(u"panelScroll"_s);
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(histogramBox);
    column->addLayout(top);
    column->addWidget(scroll, 1);
    setFixedWidth(316);
    refreshGroups();
    refreshCollapseAll();
}

AdjustPanel::~AdjustPanel() = default;

void AdjustPanel::buildSpecGroup(const GroupSpec& spec, QWidget* content, QVBoxLayout* groups)
{
    auto* group = new AdjustGroup(QString::fromLatin1(spec.name), tr(spec.name),
                                  QString::fromLatin1(spec.glyph), content);
    // Groups after Effects hold settings most photos never need: closed until opened.
    const std::string_view name(spec.name);
    group->setExpanded(name != "Optics" && name != "Calibration");
    for (const ControlSpec& c : spec.controls) {
        if (c.caption != nullptr) {
            group->addCaption(tr(c.caption), c.more);
        }
        addSpecControl(group, c);
    }
    addGroup(group, groups);
}

void AdjustPanel::addSpecControl(AdjustGroup* group, const ControlSpec& c)
{
    const QString key = QString::fromLatin1(c.key);
    const QString shown = c.label != nullptr ? tr(c.label) : QString();
    const QString full = c.fullName != nullptr ? tr(c.fullName) : shown;
    QWidget* row = nullptr;
    switch (c.kind) {
    case ControlKind::Slider: {
        AdjustSlider* s = addSlider(group, key, shown, c.more, c.display);
        if (c.fullName != nullptr) {
            s->setFullName(full);
        }
        if (c.track[0] != '\0') {
            s->slider()->setObjectName(QString::fromLatin1(c.track));
        }
        return;
    }
    case ControlKind::Choice:
        row = buildChoiceRow(key, shown, full, group);
        break;
    case ControlKind::Check:
        row = buildCheckRow(key, shown, c.note != nullptr ? tr(c.note) : QString(), full, group);
        break;
    case ControlKind::Treatment:
        row = buildTreatmentRow(group);
        break;
    case ControlKind::WhiteBalance:
        row = buildWhiteBalanceRow(group);
        break;
    }
    if (c.kind != ControlKind::WhiteBalance) {
        group->addKeys({c.key});
    }
    c.more ? group->addMoreContent(row) : group->addContent(row);
}

QWidget* AdjustPanel::buildChoiceRow(const QString& key, const QString& shown, const QString& full,
                                     QWidget* parent)
{
    const pfx::ParamSpec* p = pfx::registry().param(key.toStdString());
    Q_ASSERT(p != nullptr);
    auto* row = new ChoiceRow(shown, p->choices, choiceTexts(key.toStdString()), parent);
    row->list()->setAccessibleName(full);
    m_choices.push_back({key, row});
    connect(row, &ChoiceRow::chosen, this, [this, key, row, full](const std::string& value) {
        setChoice(key, value, u"%1 %2"_s.arg(full, row->list()->currentText()));
    });
    return row;
}

QWidget* AdjustPanel::buildCheckRow(const QString& key, const QString& shown, const QString& note,
                                    const QString& full, QWidget* parent)
{
    auto* row = new CheckRow(shown, note, parent);
    m_checks.push_back({key, row});
    connect(row, &CheckRow::toggled, this, [this, key, full](bool on) {
        //: A history step: a setting switched on or off, "Remove chromatic aberration on".
        setFlag(key, on, on ? tr("%1 on").arg(full) : tr("%1 off").arg(full));
    });
    return row;
}

QWidget* AdjustPanel::buildTreatmentRow(QWidget* parent)
{
    m_treatment = new QButtonGroup(this);
    QFrame* box = segmentBox({tr("Color"), tr("Black & white")}, 0, m_treatment, parent, SegmentFit::Equal);
    box->setToolTip(tr("Color or black and white (V)"));
    auto* row = new QWidget(parent);
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(0);
    box->setParent(row);
    layout->addWidget(box);
    connect(m_treatment, &QButtonGroup::idClicked, this, [this](int id) { setBlackAndWhite(id == 1); });
    return row;
}

void AdjustPanel::setBlackAndWhite(bool on)
{
    if (on == isBlackAndWhite()) {
        refreshTreatment();
        return;
    }
    setFlag(u"bw.enabled"_s, on, on ? tr("Black and white") : tr("Color"));
}

bool AdjustPanel::isBlackAndWhite() const
{
    return m_settings.boolean("bw.enabled", false);
}

void AdjustPanel::setChoice(const QString& key, const std::string& value, const QString& step)
{
    const pfx::ParamSpec* p = pfx::registry().param(key.toStdString());
    if (p != nullptr && !p->choices.empty() && value == p->choices.front()) {
        m_settings.erase(key.toStdString()); // defaults are not stored
    } else {
        m_settings.set(key.toStdString(), value);
    }
    refreshGroups();
    Q_EMIT settingsChanged(m_settings);
    Q_EMIT editFinished(step);
}

void AdjustPanel::setFlag(const QString& key, bool on, const QString& step)
{
    if (on) {
        m_settings.set(key.toStdString(), true);
    } else {
        m_settings.erase(key.toStdString());
    }
    refreshTreatment();
    refreshRows();
    refreshGroups();
    Q_EMIT settingsChanged(m_settings);
    Q_EMIT editFinished(step);
}

void AdjustPanel::refreshRows()
{
    for (const Bound& b : m_choices) {
        const pfx::ParamSpec* p = pfx::registry().param(b.key.toStdString());
        static_cast<ChoiceRow*>(b.row)->setValue(m_settings.text(b.key.toStdString(), p->choices.front()));
    }
    for (const Bound& b : m_checks) {
        static_cast<CheckRow*>(b.row)->setChecked(m_settings.boolean(b.key.toStdString(), false));
    }
}

void AdjustPanel::refreshTreatment()
{
    const bool bw = isBlackAndWhite();
    if (m_treatment != nullptr) {
        m_treatment->button(bw ? 1 : 0)->setChecked(true);
    }
    // Vibrance and Saturation keep their values for when colour comes back.
    for (const char* key : {"color.vibrance", "color.saturation"}) {
        if (AdjustSlider* s = slider(QString::fromLatin1(key))) {
            s->setEnabled(!bw);
            if (bw) {
                s->slider()->setToolTip(tr("No colour in black and white: kept for when you switch back"));
            } else {
                s->setFullName(s->label()); // its own tooltip back
            }
        }
    }
    if (m_mixerGroup != nullptr) {
        m_mixerGroup->setName(bw ? tr("B&W mix") : tr("Color mixer"));
        m_mixerModeRow->setVisible(!bw);
        showMixerSlider();
    }
}

ChoiceRow* AdjustPanel::choiceRow(const QString& key) const
{
    for (const Bound& b : m_choices) {
        if (b.key == key) {
            return static_cast<ChoiceRow*>(b.row);
        }
    }
    return nullptr;
}

CheckRow* AdjustPanel::checkRow(const QString& key) const
{
    for (const Bound& b : m_checks) {
        if (b.key == key) {
            return static_cast<CheckRow*>(b.row);
        }
    }
    return nullptr;
}

AdjustGroup* AdjustPanel::group(const QString& id) const
{
    for (AdjustGroup* g : m_groups) {
        if (g->id() == id) {
            return g;
        }
    }
    return nullptr;
}

void AdjustPanel::collapseAll()
{
    for (AdjustGroup* g : m_groups) {
        g->setExpanded(false);
    }
}

void AdjustPanel::expandAll()
{
    const bool solo = m_solo;
    m_solo = false; // Expand all means all, whatever the mode
    for (AdjustGroup* g : m_groups) {
        g->setExpanded(true);
    }
    m_solo = solo;
}

void AdjustPanel::openOnly(AdjustGroup* group)
{
    for (AdjustGroup* g : m_groups) {
        if (g != group) {
            const QSignalBlocker block(g); // closing them is not a reason to close more
            g->setExpanded(false);
        }
    }
    group->setExpanded(true);
    refreshCollapseAll();
}

void AdjustPanel::setOneGroupAtATime(bool on)
{
    m_solo = on;
}

void AdjustPanel::refreshCollapseAll()
{
    m_collapseAll->setEnabled(
        std::ranges::any_of(m_groups, [](const AdjustGroup* g) { return g->isExpanded(); }));
}

void AdjustPanel::showHeadingMenu(AdjustGroup* group, const QPoint& where)
{
    QMenu menu(this);
    menu.addAction(icons::themed(u"chevrons-down-up"_s, tokens().text), tr("Collapse all"), this,
                   &AdjustPanel::collapseAll);
    menu.addAction(tr("Expand all"), this, &AdjustPanel::expandAll);
    menu.addSeparator();
    QAction* solo = menu.addAction(tr("One group open at a time"));
    solo->setCheckable(true);
    solo->setChecked(m_solo);
    connect(solo, &QAction::toggled, this, [this](bool on) {
        setOneGroupAtATime(on);
        Q_EMIT oneGroupAtATimeChanged(on);
    });
    menu.addSeparator();
    // Text after a tab is the menu's right-hand hint, as a shortcut would be.
    //: A group heading's menu: open this group and close the others, "Open only Color".
    menu.addAction(buttonText(tr("Open only %1").arg(group->name())) + u'\t' + tr("Alt+click"), this,
                   [this, group] { openOnly(group); });
    menu.exec(where);
}

QStringList AdjustPanel::openMore() const
{
    QStringList ids;
    for (const AdjustGroup* g : m_groups) {
        if (g->isMoreOpen()) {
            ids.append(g->id());
        }
    }
    return ids;
}

void AdjustPanel::setOpenMore(const QStringList& ids)
{
    for (AdjustGroup* g : m_groups) {
        g->setMoreOpen(ids.contains(g->id()));
    }
}

AdjustSlider* AdjustPanel::addSlider(AdjustGroup* group, const QString& key, const QString& label, bool more,
                                     double display)
{
    auto* s = new AdjustSlider(key, label, group, display);
    more ? group->addMoreSlider(s) : group->addSlider(s);
    m_sliders.push_back(s);
    connect(s, &AdjustSlider::moved, this, [this, s](double v) { setValue(s->key(), v, false); });
    connect(s, &AdjustSlider::released, this, [this, s](double v) { setValue(s->key(), v, true); });
    return s;
}

QStringList AdjustPanel::openGroups() const
{
    QStringList ids;
    for (const AdjustGroup* group : m_groups) {
        if (group->isExpanded()) {
            ids.append(group->id());
        }
    }
    return ids;
}

void AdjustPanel::setOpenGroups(const QStringList& ids)
{
    for (AdjustGroup* group : m_groups) {
        group->setExpanded(ids.contains(group->id()));
    }
}

void AdjustPanel::addGroup(AdjustGroup* group, QVBoxLayout* groups)
{
    connect(group->resetButton(), &QToolButton::clicked, this, [this, group] { resetGroup(group); });
    connect(group, &AdjustGroup::expandedChanged, this, [this, group](bool open) {
        if (open && m_solo) {
            openOnly(group);
        }
        refreshCollapseAll();
    });
    connect(group, &AdjustGroup::soloRequested, this, [this, group] { openOnly(group); });
    connect(group, &AdjustGroup::headingMenuRequested, this,
            [this, group](const QPoint& where) { showHeadingMenu(group, where); });
    groups->addWidget(group);
    m_groups.push_back(group);
}

QWidget* AdjustPanel::buildWhiteBalanceRow(QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* label = new QLabel(tr("White balance"), row);
    label->setObjectName(u"adjustName"_s);
    m_wbMode = new QComboBox(row);
    m_wbMode->setObjectName(u"compact"_s);
    fillWhiteBalanceModes();
    m_wbMode->setAccessibleName(tr("White balance"));
    m_wbMode->setToolTip(tr("As shot keeps the camera's colours; Auto neutralises a colour cast"));
    m_picker = new QToolButton(row);
    m_picker->setObjectName(u"iconButton"_s);
    m_picker->setIcon(icons::themed(u"pipette"_s, tokens().text));
    m_picker->setCheckable(true);
    m_picker->setToolTip(tr("Pick a neutral point (W): click something that should be grey or white"));
    m_picker->setAccessibleName(tr("Pick a neutral point"));
    connect(m_picker, &QToolButton::toggled, this, &AdjustPanel::whiteBalancePickRequested);
    connect(m_wbMode, &QComboBox::activated, this, [this](int index) {
        const QVariant mode = m_wbMode->itemData(index);
        if (mode.toString() == u"asShot"_s) {
            m_settings.erase("color.wb.temperature");
            m_settings.erase("color.wb.tint");
            m_wbAuto = false;
            setSettings(m_settings);
            Q_EMIT settingsChanged(m_settings);
            Q_EMIT editFinished(tr("White balance as shot"));
        } else if (mode.toString() == u"auto"_s) {
            Q_EMIT whiteBalanceAutoRequested();
        } else if (mode.typeId() == QMetaType::Int && m_asShotKelvin > 0) {
            // A light source: its temperature relative to the as-shot white.
            const double t = std::clamp(100.0 * std::log2(mode.toInt() / static_cast<double>(m_asShotKelvin)),
                                        -100.0, 100.0);
            m_settings.set("color.wb.temperature", t);
            m_settings.erase("color.wb.tint");
            m_wbAuto = false;
            setSettings(m_settings);
            Q_EMIT settingsChanged(m_settings);
            //: A history step: a light source chosen for a RAW's white balance, "White balance Daylight".
            Q_EMIT editFinished(tr("White balance %1").arg(m_wbMode->itemText(index)));
        } else {
            refreshWhiteBalance(); // Custom is what the sliders say; picking it changes nothing
        }
    });
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 5, 0, 5);
    layout->setSpacing(kGapRelated);
    layout->addWidget(label);
    layout->addStretch();
    layout->addWidget(m_wbMode);
    layout->addWidget(m_picker);
    return row;
}

void AdjustPanel::markWhiteBalanceAuto()
{
    m_wbAuto = true;
    refreshWhiteBalance();
}

void AdjustPanel::setPicking(bool on)
{
    const QSignalBlocker block(m_picker);
    m_picker->setChecked(on);
}

void AdjustPanel::fillWhiteBalanceModes()
{
    const QSignalBlocker block(m_wbMode);
    m_wbMode->clear();
    m_wbMode->addItem(tr("As shot"), u"asShot"_s);
    m_wbMode->addItem(tr("Auto"), u"auto"_s);
    if (m_raw && m_asShotKelvin > 0) {
        // The light sources of RAW files, as Lightroom offers them.
        const std::array<std::pair<const char*, int>, 6> sources = {{{QT_TR_NOOP("Daylight"), 5500},
                                                                     {QT_TR_NOOP("Cloudy"), 6500},
                                                                     {QT_TR_NOOP("Shade"), 7500},
                                                                     {QT_TR_NOOP("Tungsten"), 2850},
                                                                     {QT_TR_NOOP("Fluorescent"), 3800},
                                                                     {QT_TR_NOOP("Flash"), 5500}}};
        for (const auto& [name, kelvin] : sources) {
            m_wbMode->addItem(tr(name), kelvin);
        }
    }
    m_wbMode->addItem(tr("Custom"), u"custom"_s);
}

void AdjustPanel::setRaw(bool raw, int asShotKelvin)
{
    m_raw = raw;
    m_asShotKelvin = raw ? asShotKelvin : 0;
    for (const auto& [key, row] : m_choices) {
        if (key == u"color.profile"_s) {
            static_cast<ChoiceRow*>(row)->setAvailable("camera", raw);
        }
    }
    for (AdjustSlider* slider : m_sliders) {
        if (slider->key() == u"color.wb.temperature"_s) {
            slider->setKelvin(m_asShotKelvin);
        }
    }
    fillWhiteBalanceModes();
    refreshWhiteBalance();
}

void AdjustPanel::refreshWhiteBalance()
{
    const bool asShot = !m_settings.contains("color.wb.temperature") && !m_settings.contains("color.wb.tint");
    if (asShot) {
        m_wbAuto = false;
    }
    QVariant mode = u"custom"_s;
    if (asShot) {
        mode = u"asShot"_s;
    } else if (m_wbAuto) {
        mode = u"auto"_s;
    } else if (m_asShotKelvin > 0 && m_settings.number("color.wb.tint", 0.0) == 0.0) {
        // A light source whose temperature the slider is at.
        const double t = m_settings.number("color.wb.temperature", 0.0);
        for (int i = 0; i < m_wbMode->count(); ++i) {
            const QVariant k = m_wbMode->itemData(i);
            if (k.typeId() == QMetaType::Int &&
                std::abs(std::clamp(100.0 * std::log2(k.toInt() / static_cast<double>(m_asShotKelvin)),
                                    -100.0, 100.0) -
                         t) < 0.5) {
                mode = k;
                break;
            }
        }
    }
    m_wbMode->setCurrentIndex(std::max(0, m_wbMode->findData(mode)));
}

void AdjustPanel::setPasteEnabled(bool enabled)
{
    m_paste->setEnabled(enabled);
}

void AdjustPanel::buildGeometryGroup(QWidget* content, QVBoxLayout* groups)
{
    auto* group = new AdjustGroup(u"Geometry"_s, tr("Geometry"), u"scaling"_s, content);
    group->setExpanded(false);
    group->addKeys({"geometry.rotate90", "geometry.flipH", "geometry.flipV"});
    auto* straighten = new AdjustSlider(u"geometry.angle"_s, tr("Straighten"), group);
    group->addSlider(straighten);
    m_sliders.push_back(straighten);
    connect(straighten, &AdjustSlider::moved, this,
            [this](double v) { editGeometry([v](pfx::Settings& s) { core::setAngle(s, v); }, {}); });
    connect(straighten, &AdjustSlider::released, this, [this, straighten](double v) {
        editGeometry([v](pfx::Settings& s) { core::setAngle(s, v); },
                     u"%1 %2"_s.arg(straighten->label(), straighten->valueText()));
    });
    struct Turn
    {
        const char* glyph;
        const char* name;
        std::function<void(pfx::Settings&)> apply;
    };
    const std::array<Turn, 4> turns = {{
        {"rotate-ccw", QT_TRANSLATE_NOOP("AdjustPanel", "Rotate left"),
         [](pfx::Settings& s) { core::turn(s, false); }},
        {"rotate-cw", QT_TRANSLATE_NOOP("AdjustPanel", "Rotate right"),
         [](pfx::Settings& s) { core::turn(s, true); }},
        {"flip-h", QT_TRANSLATE_NOOP("AdjustPanel", "Flip horizontal"),
         [](pfx::Settings& s) { core::flip(s, Qt::Horizontal); }},
        {"flip-v", QT_TRANSLATE_NOOP("AdjustPanel", "Flip vertical"),
         [](pfx::Settings& s) { core::flip(s, Qt::Vertical); }},
    }};
    auto* row = new QWidget(group);
    auto* buttons = new QHBoxLayout(row);
    buttons->setContentsMargins(0, 6, 0, 0);
    buttons->setSpacing(kGapRelated);
    for (const Turn& t : turns) {
        auto* b = new QToolButton(row);
        b->setObjectName(u"iconButton"_s);
        b->setIcon(icons::themed(QString::fromLatin1(t.glyph), tokens().text));
        b->setToolTip(tr(t.name));
        b->setAccessibleName(tr(t.name));
        connect(b, &QToolButton::clicked, this, [this, t] { editGeometry(t.apply, tr(t.name)); });
        buttons->addWidget(b);
    }
    buttons->addStretch();
    auto* crop = new QPushButton(tr("Crop and perspective"), row);
    crop->setObjectName(u"link"_s);
    crop->setToolTip(tr("Crop and rotate (C)"));
    connect(crop, &QPushButton::clicked, this, &AdjustPanel::cropRequested);
    buttons->addWidget(crop);
    group->addContent(row);
    addGroup(group, groups);
}

void AdjustPanel::editGeometry(const std::function<void(pfx::Settings&)>& change, const QString& label)
{
    change(m_settings);
    if (const AdjustSlider* s = slider(u"geometry.angle"_s); s != nullptr && !s->isChanged()) {
        m_settings.erase("geometry.angle");
    }
    refreshGroups();
    Q_EMIT settingsChanged(m_settings);
    if (!label.isEmpty()) {
        Q_EMIT editFinished(label);
    }
}

void AdjustPanel::setPhotoDetails(const QString& exposure, const QString& cameraAndLens)
{
    m_histogram->setCaption(exposure);
    m_histogram->setToolTip(cameraAndLens);
}

void AdjustPanel::setHistogram(const core::Histogram& histogram)
{
    m_histogram->setHistogram(histogram);
}

AdjustSlider* AdjustPanel::slider(const QString& key) const
{
    for (AdjustSlider* s : m_sliders) {
        if (s->key() == key) {
            return s;
        }
    }
    return nullptr;
}

void AdjustPanel::setSettings(const pfx::Settings& settings)
{
    m_settings = settings;
    for (AdjustSlider* s : m_sliders) {
        s->setValue(settings.number(s->key().toStdString(), s->defaultValue()));
    }
    followCurveMode();
    refreshControls();
    refreshRows();
    refreshTreatment();
    refreshGroups();
    refreshWhiteBalance();
}

void AdjustPanel::refreshControls()
{
    m_curve->setCurve(m_settings.curve(m_curveKey.toStdString()));
    m_regionCurve->setSettings(m_settings);
    refreshWheels();
}

void AdjustPanel::setValue(const QString& key, double value, bool finished)
{
    if (key.startsWith(u"color.wb."_s)) {
        m_wbAuto = false; // moved by hand: Custom
    }
    const AdjustSlider* s = slider(key);
    const std::string k = key.toStdString();
    if (s != nullptr && std::abs(value - s->defaultValue()) < 1e-9) {
        m_settings.erase(k); // defaults are not stored (a preset then leaves them alone)
    } else {
        m_settings.set(k, value);
    }
    if (key.startsWith(u"curves.parametric."_s)) {
        m_regionCurve->setSettings(m_settings);
    }
    if (key.startsWith(u"grading."_s)) {
        refreshWheels();
    }
    refreshGroups();
    Q_EMIT settingsChanged(m_settings);
    if (finished && s != nullptr) {
        Q_EMIT editFinished(s->isChanged() ? u"%1 %2"_s.arg(s->label(), s->valueText())
                                           : tr("Reset %1").arg(s->label()));
    }
    refreshWhiteBalance();
}

void AdjustPanel::resetGroup(AdjustGroup* group)
{
    group->reset(m_settings);
    refreshControls();
    refreshRows();
    refreshTreatment();
    refreshGroups();
    Q_EMIT settingsChanged(m_settings);
    Q_EMIT editFinished(tr("Reset %1").arg(group->name()));
}

void AdjustPanel::resetAll()
{
    m_settings = {}; // every setting, including those of controls not in this panel yet
    for (AdjustSlider* s : m_sliders) {
        s->setValue(s->defaultValue());
    }
    refreshControls();
    refreshRows();
    refreshTreatment();
    refreshGroups();
    Q_EMIT settingsChanged(m_settings);
    Q_EMIT editFinished(tr("Reset all"));
}

void AdjustPanel::refreshGroups()
{
    for (AdjustGroup* group : m_groups) {
        group->setChanged(group->isChanged(m_settings));
    }
}

} // namespace pixelfx::ui
