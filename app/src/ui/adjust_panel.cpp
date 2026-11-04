#include "ui/adjust_panel.h"

#include "ui/curve_editor.h"
#include "ui/histogram_view.h"
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

// The panel's order and wording (DESIGN section 3, mock editor.html). Ranges, defaults and
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

void repolish(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

} // namespace

/// A collapsible group: header (chevron, glyph, name, changed dot, reset) and its sliders.
class AdjustGroup : public QWidget
{
public:
    AdjustGroup(const QString& name, const QString& glyph, QWidget* parent)
        : QWidget(parent)
        , m_name(name)
        , m_toggle(new QToolButton(this))
        , m_chevron(new QLabel(this))
        , m_dot(new QLabel(this))
        , m_reset(new QToolButton(this))
        , m_body(new QWidget(this))
        , m_bodyLayout(new QVBoxLayout(m_body))
    {
        setObjectName(u"adjustGroup"_s);
        setAttribute(Qt::WA_StyledBackground);
        m_toggle->setObjectName(u"groupToggle"_s);
        m_toggle->setText(name);
        m_toggle->setIcon(icons::themed(glyph, tokens().muted));
        m_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_toggle->setCheckable(true);
        m_toggle->setChecked(true);
        m_toggle->setAccessibleName(name);
        m_toggle->setToolTip(tr("Show or hide %1").arg(name));
        m_dot->setObjectName(u"changedDot"_s);
        m_dot->setFixedSize(6, 6);
        m_dot->setToolTip(tr("Changed"));
        m_reset->setObjectName(u"groupReset"_s);
        m_reset->setIcon(icons::themed(u"reset"_s, tokens().muted));
        m_reset->setToolTip(tr("Reset %1").arg(name));
        m_reset->setAccessibleName(tr("Reset %1").arg(name));
        auto* header = new QHBoxLayout;
        header->setContentsMargins(12, 8, 10, 6);
        header->setSpacing(6);
        header->addWidget(m_chevron);
        header->addWidget(m_toggle);
        header->addWidget(m_dot);
        header->addStretch();
        header->addWidget(m_reset);
        m_bodyLayout->setContentsMargins(14, 0, 14, 10);
        m_bodyLayout->setSpacing(2);
        auto* column = new QVBoxLayout(this);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(0);
        column->addLayout(header);
        column->addWidget(m_body);
        connect(m_toggle, &QToolButton::toggled, this, [this](bool open) {
            m_body->setVisible(open);
            updateChevron();
        });
        updateChevron();
        setChanged(false);
    }

    void addContent(QWidget* widget)
    {
        widget->setParent(m_body);
        m_bodyLayout->addWidget(widget);
    }
    void addSlider(AdjustSlider* slider)
    {
        slider->setParent(m_body);
        m_bodyLayout->addWidget(slider);
        m_sliders.push_back(slider);
    }
    [[nodiscard]] const std::vector<AdjustSlider*>& sliders() const { return m_sliders; }
    [[nodiscard]] QToolButton* resetButton() const { return m_reset; }
    void setExpanded(bool open) { m_toggle->setChecked(open); }
    [[nodiscard]] const QString& name() const { return m_name; }
    void setChanged(bool changed)
    {
        m_dot->setVisible(changed);
        m_reset->setVisible(changed);
    }

private:
    void updateChevron()
    {
        m_chevron->setPixmap(icons::pixmap(m_toggle->isChecked() ? u"expanded"_s : u"collapsed"_s,
                                           tokens().muted, 14, devicePixelRatioF()));
    }

    QString m_name;
    QToolButton* m_toggle;
    QLabel* m_chevron;
    QLabel* m_dot;
    QToolButton* m_reset;
    QWidget* m_body;
    QVBoxLayout* m_bodyLayout;
    std::vector<AdjustSlider*> m_sliders;
};

// ---------------------------------------------------------------------------------------------

AdjustSlider::AdjustSlider(const QString& key, const QString& label, QWidget* parent)
    : QWidget(parent)
    , m_key(key)
    , m_label(label)
    , m_slider(new QSlider(Qt::Horizontal, this))
    , m_name(new QLabel(label, this))
    , m_value(new QLabel(this))
{
    const pfx::ParamSpec* spec = pfx::registry().param(key.toStdString());
    Q_ASSERT(spec != nullptr);
    m_default = spec->defaultValue;
    // Small ranges (exposure in stops) get hundredths; the rest whole numbers.
    m_minimum = spec->minimum;
    m_maximum = spec->maximum;
    m_degrees = spec->unit == "degrees";
    setStepsPerUnit((spec->maximum - spec->minimum) <= 20.0 ? 100.0 : 1.0);
    m_slider->setAccessibleName(label);
    m_slider->setToolTip(tr("%1. Double-click to reset.").arg(label));
    m_slider->installEventFilter(this);
    m_name->setObjectName(u"adjustName"_s);
    m_value->setObjectName(u"adjustValue"_s);
    m_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_value->setMinimumWidth(44);
    m_name->setBuddy(m_slider);

    auto* top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(m_name);
    top->addStretch();
    top->addWidget(m_value);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 4, 0, 4);
    column->setSpacing(1);
    column->addLayout(top);
    column->addWidget(m_slider);

    connect(m_slider, &QSlider::valueChanged, this, [this](int) {
        refresh();
        Q_EMIT moved(value());
        if (!m_slider->isSliderDown()) { // keyboard and wheel changes are complete at once
            Q_EMIT released(value());
        }
    });
    connect(m_slider, &QSlider::sliderReleased, this, [this] { Q_EMIT released(value()); });
    setValue(m_default);
}

AdjustSlider::~AdjustSlider() = default;

void AdjustSlider::setStepsPerUnit(double steps)
{
    const double current = value();
    m_scale = steps;
    const QSignalBlocker block(m_slider);
    m_slider->setRange(toTicks(m_minimum), toTicks(m_maximum));
    m_slider->setPageStep(static_cast<int>(m_scale > 1.0 ? m_scale : 10));
    m_slider->setValue(toTicks(current));
    refresh();
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
    return fromTicks(m_slider->value());
}

bool AdjustSlider::isChanged() const
{
    return toTicks(value()) != toTicks(m_default);
}

void AdjustSlider::setValue(double value)
{
    const QSignalBlocker block(m_slider);
    m_slider->setValue(toTicks(value));
    refresh();
}

QString AdjustSlider::valueText() const
{
    const double v = value();
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

void AdjustSlider::refresh()
{
    m_value->setText(valueText());
    const bool changed = isChanged();
    if (m_value->property("changed").toBool() != changed) {
        for (QLabel* label : {m_name, m_value}) {
            label->setProperty("changed", changed);
            repolish(label);
        }
    }
}

bool AdjustSlider::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_slider && event->type() == QEvent::MouseButtonDblClick) {
        m_slider->setValue(toTicks(m_default)); // emits moved and released
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------------------------

QHBoxLayout* AdjustPanel::buildTopRow()
{
    // Mock editor.html: Auto takes the room, then Copy, Paste and an icon-only Reset all.
    auto* resetAll = new QPushButton(icons::themed(u"reset"_s, tokens().text), QString(), this);
    resetAll->setObjectName(u"small"_s);
    resetAll->setToolTip(tr("Reset all adjustments (Ctrl+R)"));
    resetAll->setAccessibleName(tr("Reset all"));
    auto* copy = new QPushButton(icons::themed(u"copy"_s, tokens().text), tr("Copy"), this);
    copy->setObjectName(u"small"_s);
    copy->setToolTip(tr("Copy these settings, crop excepted (Ctrl+Shift+C)"));
    connect(copy, &QPushButton::clicked, this, &AdjustPanel::copyRequested);
    m_paste = new QPushButton(tr("Paste"), this);
    m_paste->setObjectName(u"small"_s);
    m_paste->setToolTip(tr("Paste copied settings onto this photo (Ctrl+Shift+V)"));
    m_paste->setEnabled(false);
    connect(m_paste, &QPushButton::clicked, this, &AdjustPanel::pasteRequested);
    connect(resetAll, &QPushButton::clicked, this, &AdjustPanel::resetAll);
    auto* autoButton = new QPushButton(icons::themed(u"auto"_s, tokens().text), tr("Auto"), this);
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
            auto* s = new AdjustSlider(QString::fromLatin1(c.key), tr(c.label), group);
            if (c.track[0] != '\0') {
                s->findChild<QSlider*>()->setObjectName(QString::fromLatin1(c.track));
            }
            group->addSlider(s);
            m_sliders.push_back(s);
            connect(s, &AdjustSlider::moved, this, [this, s](double v) { setValue(s->key(), v, false); });
            connect(s, &AdjustSlider::released, this, [this, s](double v) { setValue(s->key(), v, true); });
        }
        connect(group->resetButton(), &QToolButton::clicked, this, [this, group] { resetGroup(group); });
        groups->addWidget(group);
        m_groups.push_back(group);
        if (std::string_view(g.name) == "Color") {
            buildMixerGroup(content, groups); // mock order: Light, Color, Color mixer, Curves, Detail
            buildCurvesGroup(content, groups);
        }
    }
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

QWidget* AdjustPanel::buildWhiteBalanceRow(QWidget* parent)
{
    // Mock editor.html: "White balance" with a menu and a picker, above Temperature. Kelvin
    // presets (Daylight, Cloudy ...) need the camera's own white balance and come with RAW.
    auto* row = new QWidget(parent);
    auto* label = new QLabel(tr("White balance"), row);
    label->setObjectName(u"adjustName"_s);
    m_wbMode = new QComboBox(row);
    m_wbMode->addItems({tr("As shot"), tr("Auto"), tr("Custom")});
    m_wbMode->setAccessibleName(tr("White balance"));
    m_wbMode->setToolTip(tr("As shot keeps the camera's colours; Auto neutralises a colour cast"));
    m_picker = new QToolButton(row);
    m_picker->setObjectName(u"groupReset"_s);
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
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(4);
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
    QColor swatch;     // mock editor.html .swatches
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
    group->setExpanded(false); // mock: closed until needed
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
            auto* s = new AdjustSlider(key, label, group);
            group->addSlider(s);
            m_sliders.push_back(s);
            m_mixerSliders.push_back(s);
            connect(s, &AdjustSlider::moved, this, [this, s](double v) { setValue(s->key(), v, false); });
            connect(s, &AdjustSlider::released, this, [this, s](double v) { setValue(s->key(), v, true); });
        }
    }
    connect(m_mixerModes, &QButtonGroup::idClicked, this, &AdjustPanel::showMixerSlider);
    connect(m_mixerBands, &QButtonGroup::idClicked, this, &AdjustPanel::showMixerSlider);
    connect(group->resetButton(), &QToolButton::clicked, this, [this, group] { resetGroup(group); });
    groups->addWidget(group);
    m_groups.push_back(group);
    showMixerSlider();
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
    m_curvesGroup = new AdjustGroup(tr("Curves"), u"curves"_s, content);
    auto* channels = new QWidget(m_curvesGroup);
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
    m_curve = new CurveEditor(m_curvesGroup);
    auto* hint = new QLabel(tr("Double-click a point to remove it"), m_curvesGroup);
    hint->setObjectName(u"faint"_s);
    m_curvesGroup->addContent(channels);
    m_curvesGroup->addContent(m_curve);
    m_curvesGroup->addContent(hint);
    connect(m_curve, &CurveEditor::curveChanged, this, &AdjustPanel::curveEdited);
    connect(m_curve, &CurveEditor::editFinished, this, [this] {
        const auto* channel = m_curveChannels->checkedButton();
        Q_EMIT editFinished(m_curveKey == u"curves.rgb"_s ? tr("Curve")
                                                          : tr("%1 curve").arg(channel->text()));
    });
    connect(m_curvesGroup->resetButton(), &QToolButton::clicked, this, [this] {
        for (const QString& key : curveKeys()) {
            m_settings.erase(key.toStdString());
        }
        m_curve->setCurve({});
        refreshGroups();
        Q_EMIT settingsChanged(m_settings);
        Q_EMIT editFinished(tr("Reset Curves"));
    });
    groups->addWidget(m_curvesGroup);
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
    m_curve->setCurve(settings.curve(m_curveKey.toStdString()));
    refreshGroups();
    refreshWhiteBalance();
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
    for (AdjustSlider* s : group->sliders()) {
        m_settings.erase(s->key().toStdString());
        s->setValue(s->defaultValue());
    }
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
    m_curve->setCurve({});
    refreshGroups();
    Q_EMIT settingsChanged(m_settings);
    Q_EMIT editFinished(tr("Reset all"));
}

void AdjustPanel::refreshGroups()
{
    for (AdjustGroup* group : m_groups) {
        const auto& sliders = group->sliders();
        group->setChanged(std::ranges::any_of(sliders, [](const AdjustSlider* s) { return s->isChanged(); }));
    }
    if (m_curvesGroup != nullptr) {
        m_curvesGroup->setChanged(std::ranges::any_of(
            curveKeys(), [this](const QString& k) { return m_settings.contains(k.toStdString()); }));
    }
}

} // namespace pixelfx::ui
