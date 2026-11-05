#include "ui/adjust_panel.h"

#include "core/crop_geometry.h"
#include "ui/adjust_group.h"
#include "ui/color_wheel.h"
#include "ui/curve_editor.h"
#include "ui/histogram_view.h"
#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>
#include <cmath>
#include <functional>
#include <pfx/registry.h>
#include <string_view>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

struct ControlSpec
{
    const char* key;
    const char* label;
    const char* track; // object name for a meaningful track gradient, or empty
};

struct GroupSpec
{
    const char* name;
    const char* glyph;
    std::vector<ControlSpec> controls;
};

// The panel's order and wording (DESIGN section 3). Ranges, defaults and
// units come from the registry; display names live here, translatable (ADR-005).
const std::vector<GroupSpec>& groupSpecs()
{
    static const std::vector<GroupSpec> specs = {
        {QT_TRANSLATE_NOOP("AdjustPanel", "Light"),
         "sun",
         {{"light.exposure", QT_TRANSLATE_NOOP("AdjustPanel", "Exposure"), ""},
          {"light.contrast", QT_TRANSLATE_NOOP("AdjustPanel", "Contrast"), ""},
          {"light.highlights", QT_TRANSLATE_NOOP("AdjustPanel", "Highlights"), ""},
          {"light.shadows", QT_TRANSLATE_NOOP("AdjustPanel", "Shadows"), ""},
          {"light.whites", QT_TRANSLATE_NOOP("AdjustPanel", "Whites"), ""},
          {"light.blacks", QT_TRANSLATE_NOOP("AdjustPanel", "Blacks"), ""}}},
        {QT_TRANSLATE_NOOP("AdjustPanel", "Color"),
         "droplet",
         {{"color.wb.temperature", QT_TRANSLATE_NOOP("AdjustPanel", "Temperature"), "temperatureTrack"},
          {"color.wb.tint", QT_TRANSLATE_NOOP("AdjustPanel", "Tint"), "tintTrack"},
          {"color.vibrance", QT_TRANSLATE_NOOP("AdjustPanel", "Vibrance"), ""},
          {"color.saturation", QT_TRANSLATE_NOOP("AdjustPanel", "Saturation"), "saturationTrack"}}},
        {QT_TRANSLATE_NOOP("AdjustPanel", "Detail"),
         "detail",
         {{"detail.clarity", QT_TRANSLATE_NOOP("AdjustPanel", "Clarity"), ""},
          {"detail.sharpen.amount", QT_TRANSLATE_NOOP("AdjustPanel", "Sharpen"), ""},
          {"detail.denoise.luminance", QT_TRANSLATE_NOOP("AdjustPanel", "Noise reduction"), ""},
          {"detail.denoise.color", QT_TRANSLATE_NOOP("AdjustPanel", "Color noise"), ""},
          {"detail.dehaze", QT_TRANSLATE_NOOP("AdjustPanel", "Dehaze"), ""}}},
        {QT_TRANSLATE_NOOP("AdjustPanel", "Effects"),
         "sparkles",
         {{"effects.vignette.amount", QT_TRANSLATE_NOOP("AdjustPanel", "Vignette"), ""},
          {"effects.grain.amount", QT_TRANSLATE_NOOP("AdjustPanel", "Grain"), ""},
          {"effects.fade", QT_TRANSLATE_NOOP("AdjustPanel", "Fade"), ""}}},
    };
    return specs;
}


} // namespace

// ---------------------------------------------------------------------------------------------

AdjustSlider::AdjustSlider(const QString& key, const QString& label, QWidget* parent)
    : SliderRow(label, parent)
    , m_key(key)
{
    const pfx::ParamSpec* spec = pfx::registry().param(key.toStdString());
    Q_ASSERT(spec != nullptr);
    m_default = spec->defaultValue;
    m_minimum = spec->minimum;
    m_maximum = spec->maximum;
    m_degrees = spec->unit == "degrees";
    setFormat([this](int ticks) { return format(ticks); });
    // Small ranges (exposure in stops) get hundredths; the rest whole numbers.
    setStepsPerUnit((spec->maximum - spec->minimum) <= 20.0 ? 100.0 : 1.0);
    connect(this, &SliderRow::ticksMoved, this, [this](int ticks) { Q_EMIT moved(fromTicks(ticks)); });
    connect(this, &SliderRow::ticksReleased, this, [this](int ticks) { Q_EMIT released(fromTicks(ticks)); });
}

AdjustSlider::~AdjustSlider() = default;

void AdjustSlider::setStepsPerUnit(double steps)
{
    const double current = value();
    m_scale = steps;
    setRange(toTicks(m_minimum), toTicks(m_maximum), toTicks(m_default));
    // Exposure's keys step a twentieth of a stop, a stop with Page Up and Down; whole numbers 1 and 10.
    slider()->setSingleStep(m_scale >= 100.0 ? 5 : 1);
    slider()->setPageStep(static_cast<int>(m_scale > 1.0 ? m_scale : 10));
    setTicks(toTicks(current));
}

int AdjustSlider::toTicks(double value) const
{
    return static_cast<int>(std::lround(value * m_scale));
}

double AdjustSlider::fromTicks(int ticks) const
{
    return static_cast<double>(ticks) / m_scale;
}

double AdjustSlider::value() const
{
    return fromTicks(ticks());
}

void AdjustSlider::setValue(double value)
{
    setTicks(toTicks(value));
}

QString AdjustSlider::format(int ticks) const
{
    const double v = fromTicks(ticks);
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
    return v > 0.0 ? u"+"_s + number : number;
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
    top->setContentsMargins(12, 6, 12, 8);
    top->setSpacing(6);
    top->addWidget(autoButton, 1);
    top->addWidget(copy);
    top->addWidget(m_paste);
    top->addWidget(resetAll);
    return top;
}

AdjustPanel::AdjustPanel(QWidget* parent)
    : QWidget(parent)
    , m_histogram(new HistogramView(this))
{
    setObjectName(u"adjustPanel"_s);
    setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout* top = buildTopRow();
    auto* histogramBox = new QVBoxLayout;
    histogramBox->setContentsMargins(12, 10, 12, 0);
    histogramBox->addWidget(m_histogram);

    auto* content = new QWidget(this);
    content->setObjectName(u"adjustContent"_s);
    auto* groups = new QVBoxLayout(content);
    groups->setContentsMargins(0, 0, 0, 0);
    groups->setSpacing(0);
    for (const GroupSpec& g : groupSpecs()) {
        auto* group = new AdjustGroup(tr(g.name), QString::fromLatin1(g.glyph), content);
        if (std::string_view(g.name) == "Color") {
            group->addContent(buildWhiteBalanceRow(group));
        }
        for (const ControlSpec& c : g.controls) {
            const AdjustSlider* s = addSlider(group, QString::fromLatin1(c.key), tr(c.label));
            if (c.track[0] != '\0') {
                s->findChild<QSlider*>()->setObjectName(QString::fromLatin1(c.track));
            }
        }
        addGroup(group, groups);
        if (std::string_view(g.name) == "Color") {
            buildMixerGroup(content, groups);
            buildGradingGroup(content, groups);
            buildCurvesGroup(content, groups);
        }
    }
    buildGeometryGroup(content, groups);
    groups->addStretch();

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(u"adjustScroll"_s);
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
}

AdjustPanel::~AdjustPanel() = default;

AdjustSlider* AdjustPanel::addSlider(AdjustGroup* group, const QString& key, const QString& label)
{
    auto* s = new AdjustSlider(key, label, group);
    group->addSlider(s);
    m_sliders.push_back(s);
    connect(s, &AdjustSlider::moved, this, [this, s](double v) { setValue(s->key(), v, false); });
    connect(s, &AdjustSlider::released, this, [this, s](double v) { setValue(s->key(), v, true); });
    return s;
}

void AdjustPanel::addGroup(AdjustGroup* group, QVBoxLayout* groups)
{
    connect(group->resetButton(), &QToolButton::clicked, this, [this, group] { resetGroup(group); });
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
    m_wbMode->addItems({tr("As shot"), tr("Auto"), tr("Custom")});
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
        if (index == 0) {
            m_settings.erase("color.wb.temperature");
            m_settings.erase("color.wb.tint");
            m_wbAuto = false;
            setSettings(m_settings);
            Q_EMIT settingsChanged(m_settings);
            Q_EMIT editFinished(tr("White balance as shot"));
        } else if (index == 1) {
            Q_EMIT whiteBalanceAutoRequested();
        } else {
            refreshWhiteBalance(); // Custom is what the sliders say; picking it changes nothing
        }
    });
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 5, 0, 5);
    layout->setSpacing(6);
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

void AdjustPanel::refreshWhiteBalance()
{
    const bool asShot = !m_settings.contains("color.wb.temperature") && !m_settings.contains("color.wb.tint");
    if (asShot) {
        m_wbAuto = false;
    }
    int index = 2; // Custom
    if (asShot) {
        index = 0;
    } else if (m_wbAuto) {
        index = 1;
    }
    m_wbMode->setCurrentIndex(index);
}

void AdjustPanel::setPasteEnabled(bool enabled)
{
    m_paste->setEnabled(enabled);
}

namespace {

struct CurveChannel
{
    const char* key;
    const char* label;
    QColor color;
};

const std::vector<CurveChannel>& curveChannels()
{
    static const std::vector<CurveChannel> channels = {
        {"curves.rgb", QT_TRANSLATE_NOOP("AdjustPanel", "RGB"), QColor()},
        {"curves.r", QT_TRANSLATE_NOOP("AdjustPanel", "R"), QColor(0xE0, 0x50, 0x50)},
        {"curves.g", QT_TRANSLATE_NOOP("AdjustPanel", "G"), QColor(0x50, 0xC0, 0x60)},
        {"curves.b", QT_TRANSLATE_NOOP("AdjustPanel", "B"), QColor(0x50, 0x80, 0xF0)},
    };
    return channels;
}

const std::vector<QString>& curveKeys()
{
    static const std::vector<QString> keys = {u"curves.rgb"_s, u"curves.r"_s, u"curves.g"_s, u"curves.b"_s};
    return keys;
}

} // namespace

namespace {

struct MixerBand
{
    const char* key;
    const char* label; // plural, as in "Yellows hue"
    QColor swatch;
};

constexpr std::array<const char*, 3> kMixerModes = {"hue", "saturation", "luminance"};

const std::vector<MixerBand>& mixerBands()
{
    static const std::vector<MixerBand> bands = {
        {"red", QT_TRANSLATE_NOOP("AdjustPanel", "Reds"), QColor(0xE0, 0x4F, 0x4F)},
        {"orange", QT_TRANSLATE_NOOP("AdjustPanel", "Oranges"), QColor(0xE8, 0x90, 0x4A)},
        {"yellow", QT_TRANSLATE_NOOP("AdjustPanel", "Yellows"), QColor(0xE0, 0xC0, 0x4F)},
        {"green", QT_TRANSLATE_NOOP("AdjustPanel", "Greens"), QColor(0x4F, 0xC0, 0x60)},
        {"aqua", QT_TRANSLATE_NOOP("AdjustPanel", "Aquas"), QColor(0x4F, 0xC0, 0xC0)},
        {"blue", QT_TRANSLATE_NOOP("AdjustPanel", "Blues"), QColor(0x4F, 0x80, 0xE0)},
        {"purple", QT_TRANSLATE_NOOP("AdjustPanel", "Purples"), QColor(0xA0, 0x4F, 0xE0)},
        {"magenta", QT_TRANSLATE_NOOP("AdjustPanel", "Magentas"), QColor(0xE0, 0x4F, 0xB0)},
    };
    return bands;
}

} // namespace

void AdjustPanel::buildMixerGroup(QWidget* content, QVBoxLayout* groups)
{
    auto* group = new AdjustGroup(tr("Color mixer"), u"palette"_s, content);
    group->setExpanded(false);
    auto* modes = new QWidget(group);
    auto* modeRow = new QHBoxLayout(modes);
    modeRow->setContentsMargins(0, 4, 0, 8);
    modeRow->setSpacing(0);
    m_mixerModes = new QButtonGroup(this);
    const QStringList modeNames = {tr("Hue"), tr("Saturation"), tr("Luminance")};
    for (int i = 0; i < modeNames.size(); ++i) {
        auto* b = new QPushButton(modeNames.at(i), modes);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        b->setChecked(i == 0);
        m_mixerModes->addButton(b, i);
        modeRow->addWidget(b, 1);
    }
    auto* swatches = new QWidget(group);
    auto* swatchRow = new QHBoxLayout(swatches);
    swatchRow->setContentsMargins(0, 0, 0, 6);
    swatchRow->setSpacing(6);
    m_mixerBands = new QButtonGroup(this);
    const std::vector<MixerBand>& bands = mixerBands();
    for (std::size_t i = 0; i < bands.size(); ++i) {
        auto* s = new QToolButton(swatches);
        s->setObjectName(u"swatch"_s);
        s->setCheckable(true);
        s->setChecked(i == 0);
        s->setFixedSize(24, 24);
        s->setStyleSheet(u"QToolButton#swatch { background: %1; }"_s.arg(bands.at(i).swatch.name()));
        s->setToolTip(tr(bands.at(i).label));
        s->setAccessibleName(tr(bands.at(i).label));
        m_mixerBands->addButton(s, static_cast<int>(i));
        swatchRow->addWidget(s);
    }
    swatchRow->addStretch();
    group->addContent(modes);
    group->addContent(swatches);
    for (std::size_t m = 0; m < kMixerModes.size(); ++m) {
        for (const MixerBand& band : bands) {
            const QString key =
                u"mixer.%1.%2"_s.arg(QLatin1StringView(kMixerModes.at(m)), QLatin1StringView(band.key));
            //: A colour band and what changes in it, for example "Yellows hue".
            const QString label =
                tr("%1 %2").arg(tr(band.label), modeNames.at(static_cast<qsizetype>(m)).toLower());
            m_mixerSliders.push_back(addSlider(group, key, label));
        }
    }
    connect(m_mixerModes, &QButtonGroup::idClicked, this, &AdjustPanel::showMixerSlider);
    connect(m_mixerBands, &QButtonGroup::idClicked, this, &AdjustPanel::showMixerSlider);
    addGroup(group, groups);
    showMixerSlider();
}

namespace {

struct GradingZone
{
    const char* key;   // grading.<key>.hue and grading.<key>.amount
    const char* label; // above the wheel
    const char* name;  // what screen readers and the history hear
};

constexpr std::array<GradingZone, 2> kGradingZones = {{
    {"shadows", QT_TRANSLATE_NOOP("AdjustPanel", "Shadows"),
     QT_TRANSLATE_NOOP("AdjustPanel", "Shadows colour")},
    {"highlights", QT_TRANSLATE_NOOP("AdjustPanel", "Highlights"),
     QT_TRANSLATE_NOOP("AdjustPanel", "Highlights colour")},
}};

std::string gradingKey(int zone, const char* part)
{
    return std::string("grading.") + kGradingZones.at(static_cast<std::size_t>(zone)).key + "." + part;
}

} // namespace

void AdjustPanel::buildGradingGroup(QWidget* content, QVBoxLayout* groups)
{
    auto* group = new AdjustGroup(tr("Color grading"), u"blend"_s, content);
    group->setExpanded(false);
    auto* wheels = new QWidget(group);
    auto* row = new QHBoxLayout(wheels);
    row->setContentsMargins(0, 6, 0, 4);
    row->setSpacing(8);
    for (int zone = 0; zone < 2; ++zone) {
        row->addWidget(buildWheel(zone, wheels), 1);
    }
    group->addContent(wheels);
    for (int zone = 0; zone < 2; ++zone) {
        group->addKeys({gradingKey(zone, "hue"), gradingKey(zone, "amount")});
    }
    AdjustSlider* balance = addSlider(group, u"grading.balance"_s, tr("Balance"));
    balance->setToolTip(tr("Where shadows end and highlights begin: to the right gives the highlight "
                           "colour more of the photo."));
    addGroup(group, groups);
    refreshWheels();
}

QWidget* AdjustPanel::buildWheel(int zone, QWidget* parent)
{
    const GradingZone& z = kGradingZones.at(static_cast<std::size_t>(zone));
    auto* box = new QWidget(parent);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(6);
    auto* title = new QLabel(tr(z.label), box);
    title->setObjectName(u"wheelTitle"_s);
    title->setAlignment(Qt::AlignCenter);
    auto* wheel = new ColorWheel(tr(z.name), box);
    wheel->setObjectName(u"gradingWheel"_s);
    auto* values = new QLabel(box);
    values->setObjectName(u"wheelValues"_s);
    values->setAlignment(Qt::AlignCenter);
    values->setTextFormat(Qt::RichText);
    column->addWidget(title);
    column->addWidget(wheel, 0, Qt::AlignHCenter);
    column->addWidget(values);
    const auto i = static_cast<std::size_t>(zone);
    m_wheels.at(i) = wheel;
    m_wheelValues.at(i) = values;
    connect(wheel, &ColorWheel::moved, this,
            [this, zone](double hue, double amount) { wheelEdited(zone, hue, amount, false); });
    connect(wheel, &ColorWheel::released, this,
            [this, zone](double hue, double amount) { wheelEdited(zone, hue, amount, true); });
    return box;
}

ColorWheel* AdjustPanel::gradingWheel(int zone) const
{
    return m_wheels.at(static_cast<std::size_t>(zone));
}

void AdjustPanel::wheelEdited(int zone, double hue, double amount, bool finished)
{
    const std::string hueKey = gradingKey(zone, "hue");
    const std::string amountKey = gradingKey(zone, "amount");
    // Defaults are not stored (a preset then leaves them alone); the hue is kept with no amount
    // so the wheel remembers where it was.
    if (hue == 0.0) {
        m_settings.erase(hueKey);
    } else {
        m_settings.set(hueKey, hue);
    }
    if (amount == 0.0) {
        m_settings.erase(amountKey);
    } else {
        m_settings.set(amountKey, amount);
    }
    refreshWheels();
    refreshGroups();
    Q_EMIT settingsChanged(m_settings);
    if (finished) {
        const QString name = tr(kGradingZones.at(static_cast<std::size_t>(zone)).name);
        //: A history step: a colour grading wheel and its new hue and amount, "Shadows colour 200°, 20".
        Q_EMIT editFinished(amount == 0.0 && hue == 0.0
                                ? tr("Reset %1").arg(name)
                                : tr("%1 %2°, %3").arg(name).arg(qRound(hue)).arg(qRound(amount)));
    }
}

void AdjustPanel::refreshWheels()
{
    for (int zone = 0; zone < 2; ++zone) {
        const auto i = static_cast<std::size_t>(zone);
        const double hue = m_settings.number(gradingKey(zone, "hue"), 0.0);
        const double amount = m_settings.number(gradingKey(zone, "amount"), 0.0);
        m_wheels.at(i)->setColor(hue, amount);
        const QString amountText =
            amount != 0.0
                ? u"<span style=\"color:%1\">%2</span>"_s.arg(tokens().accent.name()).arg(qRound(amount))
                : QString::number(qRound(amount));
        //: Under a colour grading wheel: "Hue 200° · Amount 20".
        m_wheelValues.at(i)->setText(
            tr("<span style=\"color:%1\">Hue</span> %2° · <span style=\"color:%1\">Amount</span> %3")
                .arg(tokens().muted.name())
                .arg(qRound(m_wheels.at(i)->hue()))
                .arg(amountText));
    }
}

void AdjustPanel::buildGeometryGroup(QWidget* content, QVBoxLayout* groups)
{
    auto* group = new AdjustGroup(tr("Geometry"), u"scaling"_s, content);
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
    buttons->setSpacing(6);
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

void AdjustPanel::showMixerSlider()
{
    const std::size_t shown = (static_cast<std::size_t>(m_mixerModes->checkedId()) * mixerBands().size()) +
                              static_cast<std::size_t>(m_mixerBands->checkedId());
    for (std::size_t i = 0; i < m_mixerSliders.size(); ++i) {
        m_mixerSliders.at(i)->setVisible(i == shown);
    }
}

AdjustSlider* AdjustPanel::mixerSlider() const
{
    for (AdjustSlider* s : m_mixerSliders) {
        if (!s->isHidden()) {
            return s;
        }
    }
    return nullptr;
}

void AdjustPanel::showMixer(int mode, int band)
{
    m_mixerModes->button(mode)->setChecked(true);
    m_mixerBands->button(band)->setChecked(true);
    showMixerSlider();
}

void AdjustPanel::buildCurvesGroup(QWidget* content, QVBoxLayout* groups)
{
    auto* group = new AdjustGroup(tr("Curves"), u"curves"_s, content);
    group->setExpanded(false);
    for (const QString& key : curveKeys()) {
        group->addKeys({key.toStdString()});
    }
    auto* channels = new QWidget(group);
    auto* channelRow = new QHBoxLayout(channels);
    channelRow->setContentsMargins(0, 4, 0, 4);
    channelRow->setSpacing(0);
    m_curveChannels = new QButtonGroup(this);
    for (const CurveChannel& c : curveChannels()) {
        auto* b = new QPushButton(tr(c.label), channels);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        b->setChecked(QLatin1StringView(c.key) == m_curveKey);
        b->setProperty("curveKey", QString::fromLatin1(c.key));
        if (c.color.isValid()) {
            b->setStyleSheet(u"color: %1;"_s.arg(c.color.name()));
        }
        m_curveChannels->addButton(b);
        channelRow->addWidget(b, 1);
    }
    connect(m_curveChannels, &QButtonGroup::buttonClicked, this,
            [this](QAbstractButton* b) { showCurve(b->property("curveKey").toString()); });
    m_curve = new CurveEditor(group);
    auto* hint = new QLabel(tr("Double-click a point to remove it"), group);
    hint->setObjectName(u"faint"_s);
    group->addContent(channels);
    group->addContent(m_curve);
    group->addContent(hint);
    connect(m_curve, &CurveEditor::curveChanged, this, &AdjustPanel::curveEdited);
    connect(m_curve, &CurveEditor::editFinished, this, [this] {
        const auto* channel = m_curveChannels->checkedButton();
        Q_EMIT editFinished(m_curveKey == u"curves.rgb"_s ? tr("Curve")
                                                          : tr("%1 curve").arg(channel->text()));
    });
    addGroup(group, groups);
}

void AdjustPanel::showCurve(const QString& key)
{
    m_curveKey = key;
    for (const CurveChannel& c : curveChannels()) {
        if (key == QLatin1StringView(c.key)) {
            m_curve->setColor(c.color.isValid() ? c.color : tokens().text);
        }
    }
    for (QAbstractButton* b : m_curveChannels->buttons()) {
        b->setChecked(b->property("curveKey").toString() == key);
    }
    m_curve->setCurve(m_settings.curve(key.toStdString()));
}

void AdjustPanel::curveEdited(const pfx::Curve& curve)
{
    if (m_curve->isIdentity()) {
        m_settings.erase(m_curveKey.toStdString());
    } else {
        m_settings.set(m_curveKey.toStdString(), curve);
    }
    refreshGroups();
    Q_EMIT settingsChanged(m_settings);
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
    refreshControls();
    refreshGroups();
    refreshWhiteBalance();
}

void AdjustPanel::refreshControls()
{
    m_curve->setCurve(m_settings.curve(m_curveKey.toStdString()));
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
