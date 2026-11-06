// The colour groups of the adjust panel that are more than sliders: the Color mixer (the B&W mix
// in black and white), Color grading and Curves.
#include "ui/adjust_group.h"
#include "ui/adjust_panel.h"
#include "ui/color_swatch.h"
#include "ui/color_wheel.h"
#include "ui/curve_editor.h"
#include "ui/grading_range_button.h"
#include "ui/icons.h"
#include "ui/region_curve.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

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

struct GradingRange
{
    const char* key;   // grading.<key>.hue, .amount, .luminance
    const char* label; // under its small wheel
    const char* name;  // what screen readers and the history hear for the large wheel
};

constexpr std::array<GradingRange, 4> kGradingRanges = {{
    {"shadows", QT_TRANSLATE_NOOP("AdjustPanel", "Shadows"),
     QT_TRANSLATE_NOOP("AdjustPanel", "Shadows colour")},
    {"midtones", QT_TRANSLATE_NOOP("AdjustPanel", "Midtones"),
     QT_TRANSLATE_NOOP("AdjustPanel", "Midtones colour")},
    {"highlights", QT_TRANSLATE_NOOP("AdjustPanel", "Highlights"),
     QT_TRANSLATE_NOOP("AdjustPanel", "Highlights colour")},
    {"global", QT_TRANSLATE_NOOP("AdjustPanel", "Global"), QT_TRANSLATE_NOOP("AdjustPanel", "Global colour")},
}};

std::string gradingKey(int range, const char* part)
{
    return std::string("grading.") + kGradingRanges.at(static_cast<std::size_t>(range)).key + "." + part;
}

constexpr int kLargeWheel = 120;

struct CurveChannel
{
    const char* key;
    const char* label;
    const char* channel; // "red", "green", "blue", or nullptr for all three
};

const std::vector<CurveChannel>& curveChannels()
{
    static const std::vector<CurveChannel> channels = {
        {"curves.rgb", QT_TRANSLATE_NOOP("AdjustPanel", "RGB"), nullptr},
        {"curves.r", QT_TRANSLATE_NOOP("AdjustPanel", "R"), "red"},
        {"curves.g", QT_TRANSLATE_NOOP("AdjustPanel", "G"), "green"},
        {"curves.b", QT_TRANSLATE_NOOP("AdjustPanel", "B"), "blue"},
    };
    return channels;
}

// The colour a curve is drawn in: its channel's, or the text colour for all three.
QColor channelColor(const char* channel)
{
    const Tokens& t = tokens();
    if (channel == nullptr) {
        return t.text;
    }
    const std::string_view c(channel);
    if (c == "red") {
        return t.channelRed;
    }
    return c == "green" ? t.channelGreen : t.channelBlue;
}

const std::vector<QString>& curveKeys()
{
    static const std::vector<QString> keys = {u"curves.rgb"_s, u"curves.r"_s, u"curves.g"_s, u"curves.b"_s};
    return keys;
}

struct Region
{
    const char* key;
    const char* label;
    const char* name;
};

// Top to bottom as Lightroom lists them: the brightest region first.
constexpr std::array<Region, 4> kRegions = {{
    {"curves.parametric.highlights", QT_TRANSLATE_NOOP("AdjustPanel", "Highlights"),
     QT_TRANSLATE_NOOP("AdjustPanel", "Highlights region")},
    {"curves.parametric.lights", QT_TRANSLATE_NOOP("AdjustPanel", "Lights"),
     QT_TRANSLATE_NOOP("AdjustPanel", "Lights region")},
    {"curves.parametric.darks", QT_TRANSLATE_NOOP("AdjustPanel", "Darks"),
     QT_TRANSLATE_NOOP("AdjustPanel", "Darks region")},
    {"curves.parametric.shadows", QT_TRANSLATE_NOOP("AdjustPanel", "Shadows"),
     QT_TRANSLATE_NOOP("AdjustPanel", "Shadows region")},
}};

constexpr std::array<const char*, 3> kSplitKeys = {"curves.parametric.split.shadows",
                                                   "curves.parametric.split.midtones",
                                                   "curves.parametric.split.highlights"};
constexpr std::array<double, 3> kSplitDefaults = {25.0, 50.0, 75.0};

} // namespace

// --- Color mixer and the B&W mix ---------------------------------------------------------------

QWidget* AdjustPanel::buildMixerSwatches(QWidget* parent)
{
    auto* swatches = new QWidget(parent);
    auto* swatchRow = new QHBoxLayout(swatches);
    swatchRow->setContentsMargins(0, 4, 0, 8); // `.swatches` margins
    swatchRow->setSpacing(kGapRelated);
    m_mixerBands = new QButtonGroup(this);
    const std::vector<MixerBand>& bands = mixerBands();
    for (std::size_t i = 0; i < bands.size(); ++i) {
        auto* s = new ColorSwatch(bands.at(i).swatch, tr(bands.at(i).label), swatches);
        s->setChecked(i == 0);
        m_mixerBands->addButton(s, static_cast<int>(i));
        swatchRow->addWidget(s);
    }
    swatchRow->addStretch();
    m_target = new QToolButton(swatches);
    m_target->setObjectName(u"iconButton"_s);
    m_target->setIcon(icons::themed(u"target"_s, tokens().text));
    m_target->setCheckable(true);
    m_target->setAccessibleName(tr("Pick a colour on the photo"));
    m_target->setToolTip(tr("Pick a colour on the photo and drag up or down to change it (T)"));
    connect(m_target, &QToolButton::toggled, this, &AdjustPanel::targetToggled);
    swatchRow->addWidget(m_target);
    return swatches;
}

void AdjustPanel::buildMixerGroup(QWidget* content, QVBoxLayout* groups)
{
    auto* group = new AdjustGroup(u"Color mixer"_s, tr("Color mixer"), u"palette"_s, content);
    m_mixerGroup = group;
    group->setExpanded(false);
    m_mixerModeRow = new QWidget(group);
    auto* modeRow = new QHBoxLayout(m_mixerModeRow);
    modeRow->setContentsMargins(0, 4, 0, 8);
    modeRow->setSpacing(0);
    m_mixerModes = new QButtonGroup(this);
    const QStringList modeNames = {tr("Hue"), tr("Saturation"), tr("Luminance")};
    for (int i = 0; i < modeNames.size(); ++i) {
        auto* b = new QPushButton(modeNames.at(i), m_mixerModeRow);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        b->setChecked(i == 0);
        m_mixerModes->addButton(b, i);
        modeRow->addWidget(b, 1);
    }
    QWidget* swatches = buildMixerSwatches(group);
    const std::vector<MixerBand>& bands = mixerBands();
    group->addContent(m_mixerModeRow);
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
    for (const MixerBand& band : bands) {
        AdjustSlider* s = addSlider(group, u"bw.mix.%1"_s.arg(QLatin1StringView(band.key)), tr(band.label));
        //: A B&W mix slider, for example "Blues in black and white".
        s->setFullName(tr("%1 in black and white").arg(tr(band.label)));
        s->slider()->setObjectName(u"bwTrack"_s);
        m_bwSliders.push_back(s);
    }
    m_targetAlso = new QLabel(group);
    m_targetAlso->setObjectName(u"muted"_s);
    m_targetAlso->hide();
    group->addContent(m_targetAlso);
    connect(m_mixerModes, &QButtonGroup::idClicked, this, &AdjustPanel::showMixerSlider);
    connect(m_mixerBands, &QButtonGroup::idClicked, this, &AdjustPanel::showMixerSlider);
    addGroup(group, groups);
    showMixerSlider();
}

// --- Picking a colour on the photo ---------------------------------------------------------------

void AdjustPanel::setTargeting(bool on)
{
    const QSignalBlocker block(m_target);
    m_target->setChecked(on);
    m_target->setToolTip(
        on ? tr("Drag on the photo to change the colour under the pointer (T). Escape stops.")
           : tr("Pick a colour on the photo and drag up or down to change it (T)"));
    if (on) {
        m_mixerGroup->setExpanded(true);
    } else {
        m_targetDrag.reset();
        m_targetAlso->hide();
    }
}

bool AdjustPanel::isTargeting() const
{
    return m_target->isChecked();
}

QString AdjustPanel::targetKey(std::size_t band) const
{
    const char* name = mixerBands().at(band).key;
    if (isBlackAndWhite()) {
        return u"bw.mix.%1"_s.arg(QLatin1StringView(name));
    }
    const auto mode = static_cast<std::size_t>(std::max(0, m_mixerModes->checkedId()));
    return u"mixer.%1.%2"_s.arg(QLatin1StringView(kMixerModes.at(mode)), QLatin1StringView(name));
}

QString AdjustPanel::mixerModeWord() const
{
    const std::array<QString, 3> modes = {tr("hue"), tr("saturation"), tr("luminance")};
    return modes.at(static_cast<std::size_t>(std::max(0, m_mixerModes->checkedId())));
}

QString AdjustPanel::targetDragWords() const
{
    if (isBlackAndWhite()) {
        return tr("Drag up or down: lighter or darker");
    }
    //: Beside the colour picked on the photo, "Drag up or down: luminance".
    return tr("Drag up or down: %1").arg(mixerModeWord());
}

QString AdjustPanel::targetBanner() const
{
    if (isBlackAndWhite()) {
        return tr("Drag on a colour to make it lighter or darker in black and white");
    }
    //: The line at the top while picking a colour, "Drag on a colour to change its luminance".
    return tr("Drag on a colour to change its %1").arg(mixerModeWord());
}

QString AdjustPanel::targetName(const pfx::HueBands& bands)
{
    constexpr double kGrey = 0.05;  // the mixer barely acts below this
    constexpr double kAlone = 0.15; // a neighbour with less than this share is not named
    if (bands.weight < kGrey) {
        return tr("No colour here");
    }
    const bool second = bands.secondShare > 0.5;
    QString main = tr(mixerBands().at(second ? bands.second : bands.first).label);
    const double otherShare = second ? 1.0 - bands.secondShare : bands.secondShare;
    if (otherShare < kAlone) {
        return main;
    }
    const QString other = tr(mixerBands().at(second ? bands.first : bands.second).label);
    //: Two colour bands under the pointer, "Blues and aquas".
    return tr("%1 and %2").arg(main, other.toLower());
}

void AdjustPanel::pickTarget(const pfx::HueBands& bands)
{
    constexpr double kGrey = 0.05;
    m_targetAlso->hide();
    if (bands.weight < kGrey) {
        m_targetDrag.reset();
        return;
    }
    const bool secondFirst = bands.secondShare > 0.5;
    TargetDrag drag;
    drag.bands = {secondFirst ? bands.second : bands.first, secondFirst ? bands.first : bands.second};
    const double main = secondFirst ? bands.secondShare : 1.0 - bands.secondShare;
    drag.ratio = (1.0 - main) / main;
    for (std::size_t i = 0; i < 2; ++i) {
        drag.start.at(i) = m_settings.number(targetKey(drag.bands.at(i)).toStdString(), 0.0);
    }
    m_targetDrag = drag;
    m_mixerBands->button(static_cast<int>(drag.bands[0]))->setChecked(true);
    showMixerSlider();
}

void AdjustPanel::dragTarget(double steps, bool finished)
{
    if (!m_targetDrag) {
        return;
    }
    TargetDrag& d = *m_targetDrag;
    std::array<double, 2> values{};
    for (std::size_t i = 0; i < 2; ++i) {
        const double change = std::round(steps * (i == 0 ? 1.0 : d.ratio));
        values.at(i) = std::clamp(d.start.at(i) + change, -100.0, 100.0);
        const std::string key = targetKey(d.bands.at(i)).toStdString();
        values.at(i) == 0.0 ? m_settings.erase(key) : m_settings.set(key, values.at(i));
        if (AdjustSlider* s = slider(targetKey(d.bands.at(i)))) {
            s->setValue(values.at(i));
        }
    }
    const bool moves = d.ratio > 0.0 && std::round(steps * d.ratio) != 0.0;
    const AdjustSlider* other = slider(targetKey(d.bands[1]));
    const QString otherName = isBlackAndWhite() ? tr(mixerBands().at(d.bands[1]).label) : other->label();
    const auto signedValue = [](double v) { return v > 0.0 ? u"+%1"_s.arg(v) : QString::number(v); };
    //: Under the colour mixer while dragging on the photo, "Also moves Aquas luminance -10".
    m_targetAlso->setText(tr("Also moves %1 %2").arg(otherName, signedValue(values[1])));
    m_targetAlso->setVisible(moves);
    refreshGroups();
    Q_EMIT settingsChanged(m_settings);
    if (finished) {
        const AdjustSlider* main = slider(targetKey(d.bands[0]));
        QString name = isBlackAndWhite() ? tr(mixerBands().at(d.bands[0]).label) : main->label();
        if (moves) {
            const QString both = tr("%1 and %2")
                                     .arg(tr(mixerBands().at(d.bands[0]).label),
                                          tr(mixerBands().at(d.bands[1]).label).toLower());
            name = isBlackAndWhite() ? both : u"%1 %2"_s.arg(both, mixerModeWord());
        }
        //: A history step of the colour picked on the photo, "Blues and aquas luminance -24".
        Q_EMIT editFinished(isBlackAndWhite()
                                ? tr("%1 in black and white %2").arg(name, signedValue(values[0]))
                                : u"%1 %2"_s.arg(name, signedValue(values[0])));
        // The next drag starts from here.
        d.start = values;
    }
}

void AdjustPanel::showMixerSlider()
{
    Q_EMIT targetModeChanged();
    const bool bw = isBlackAndWhite();
    const auto band = static_cast<std::size_t>(m_mixerBands->checkedId());
    const std::size_t shown =
        (static_cast<std::size_t>(m_mixerModes->checkedId()) * mixerBands().size()) + band;
    for (std::size_t i = 0; i < m_mixerSliders.size(); ++i) {
        m_mixerSliders.at(i)->setVisible(!bw && i == shown);
    }
    for (std::size_t i = 0; i < m_bwSliders.size(); ++i) {
        m_bwSliders.at(i)->setVisible(bw && i == band);
    }
}

AdjustSlider* AdjustPanel::mixerSlider() const
{
    for (AdjustSlider* s : m_mixerSliders) {
        if (!s->isHidden()) {
            return s;
        }
    }
    for (AdjustSlider* s : m_bwSliders) {
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

// --- Color grading -----------------------------------------------------------------------------

void AdjustPanel::buildGradingGroup(QWidget* content, QVBoxLayout* groups)
{
    auto* group = new AdjustGroup(u"Color grading"_s, tr("Color grading"), u"blend"_s, content);
    group->setExpanded(false);
    group->addContent(buildGradingRanges(group));
    m_wheel = new ColorWheel(tr(kGradingRanges.at(0).name), group, kLargeWheel);
    m_wheel->setObjectName(u"gradingWheel"_s);
    m_wheelValues = new QLabel(group);
    m_wheelValues->setObjectName(u"wheelValues"_s);
    m_wheelValues->setAlignment(Qt::AlignCenter);
    m_wheelValues->setTextFormat(Qt::RichText);
    auto* wheelBox = new QWidget(group);
    auto* column = new QVBoxLayout(wheelBox);
    // 8 px from the ranges with their own 4 px (the mock's margins meet), 6 px from the luminance.
    column->setContentsMargins(0, kGapSmall, 0, kGapTight);
    column->setSpacing(kGapRelated);
    column->addWidget(m_wheel, 0, Qt::AlignHCenter);
    column->addWidget(m_wheelValues);
    group->addContent(wheelBox);
    for (int range = 0; range < 4; ++range) {
        group->addKeys({gradingKey(range, "hue"), gradingKey(range, "amount")});
        AdjustSlider* luminance =
            addSlider(group, QString::fromStdString(gradingKey(range, "luminance")), tr("Luminance"));
        //: A colour grading range's luminance, for example "Shadows luminance".
        luminance->setFullName(
            tr("%1 luminance").arg(tr(kGradingRanges.at(static_cast<std::size_t>(range)).label)));
        m_rangeLuminance.at(static_cast<std::size_t>(range)) = luminance;
    }
    auto* ruleBox = new QWidget(group);
    auto* ruleLayout = new QVBoxLayout(ruleBox);
    ruleLayout->setContentsMargins(0, kGapControls, 0, kGapSmall);
    auto* rule = new QFrame(ruleBox);
    rule->setObjectName(u"hairline"_s);
    rule->setFixedHeight(1);
    ruleLayout->addWidget(rule);
    group->addContent(ruleBox);
    AdjustSlider* blending = addSlider(group, u"grading.blending"_s, tr("Blending"));
    blending->setToolTip(tr("How far the ranges run into each other"));
    AdjustSlider* balance = addSlider(group, u"grading.balance"_s, tr("Balance"));
    balance->setToolTip(tr("Where shadows end and highlights begin: to the right gives the highlight "
                           "colour more of the photo."));
    connect(m_wheel, &ColorWheel::moved, this,
            [this](double hue, double amount) { wheelEdited(hue, amount, false); });
    connect(m_wheel, &ColorWheel::released, this,
            [this](double hue, double amount) { wheelEdited(hue, amount, true); });
    addGroup(group, groups);
    showGradingRange(0);
}

QWidget* AdjustPanel::buildGradingRanges(QWidget* parent)
{
    auto* row = new QWidget(parent);
    row->setAccessibleName(tr("Range"));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, kGapRelated, 0, kGapSmall);
    layout->setSpacing(kGapSmall);
    m_gradingRanges = new QButtonGroup(this);
    for (int range = 0; range < 4; ++range) {
        auto* b = new GradingRangeButton(tr(kGradingRanges.at(static_cast<std::size_t>(range)).label), row);
        m_gradingRanges->addButton(b, range);
        m_rangeButtons.at(static_cast<std::size_t>(range)) = b;
        layout->addWidget(b, 1);
    }
    connect(m_gradingRanges, &QButtonGroup::idClicked, this, &AdjustPanel::showGradingRange);
    return row;
}

void AdjustPanel::showGradingRange(int range)
{
    m_gradingRanges->button(range)->setChecked(true);
    m_wheel->setName(tr(kGradingRanges.at(static_cast<std::size_t>(range)).name));
    const auto shown = static_cast<std::size_t>(range);
    for (std::size_t i = 0; i < m_rangeLuminance.size(); ++i) {
        m_rangeLuminance.at(i)->setVisible(i == shown);
    }
    refreshWheels();
}

int AdjustPanel::gradingRange() const
{
    return m_gradingRanges->checkedId();
}

void AdjustPanel::wheelEdited(double hue, double amount, bool finished)
{
    const int range = gradingRange();
    const std::string hueKey = gradingKey(range, "hue");
    const std::string amountKey = gradingKey(range, "amount");
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
        const QString name = tr(kGradingRanges.at(static_cast<std::size_t>(range)).name);
        //: A history step: a colour grading wheel and its new hue and amount, "Shadows colour 200°, 20".
        Q_EMIT editFinished(amount == 0.0 && hue == 0.0
                                ? tr("Reset %1").arg(name)
                                : tr("%1 %2°, %3").arg(name).arg(qRound(hue)).arg(qRound(amount)));
    }
}

void AdjustPanel::refreshWheels()
{
    for (int range = 0; range < 4; ++range) {
        m_rangeButtons.at(static_cast<std::size_t>(range))
            ->setColor(m_settings.number(gradingKey(range, "hue"), 0.0),
                       m_settings.number(gradingKey(range, "amount"), 0.0));
    }
    const int range = gradingRange();
    const double hue = m_settings.number(gradingKey(range, "hue"), 0.0);
    const double amount = m_settings.number(gradingKey(range, "amount"), 0.0);
    m_wheel->setColor(hue, amount);
    const QString amountText =
        amount != 0.0
            ? u"<span style=\"color:%1\">%2</span>"_s.arg(tokens().accent.name()).arg(qRound(amount))
            : QString::number(qRound(amount));
    //: Under the colour grading wheel: "Hue 200° · Amount 20".
    m_wheelValues->setText(
        tr("<span style=\"color:%1\">Hue</span> %2° · <span style=\"color:%1\">Amount</span> %3")
            .arg(tokens().muted.name())
            .arg(qRound(m_wheel->hue()))
            .arg(amountText));
}

// --- Curves ------------------------------------------------------------------------------------

void AdjustPanel::buildCurvesGroup(QWidget* content, QVBoxLayout* groups)
{
    auto* group = new AdjustGroup(u"Curves"_s, tr("Curves"), u"curves"_s, content);
    group->setExpanded(false);
    m_curveModes = new QButtonGroup(this);
    auto* modes = new QWidget(group);
    auto* modeRow = new QVBoxLayout(modes);
    modeRow->setContentsMargins(0, 6, 0, 8);
    QFrame* box = segmentBox({tr("Points"), tr("Regions")}, 0, m_curveModes, modes, SegmentFit::Equal);
    box->setToolTip(tr("Points: drag the curve itself. Regions: lift or lower four ranges of tones."));
    modeRow->addWidget(box);
    group->addContent(modes);
    group->addContent(buildCurvePoints(group));
    buildCurveRegions(group);
    connect(m_curveModes, &QButtonGroup::idClicked, this, [this](int id) { showCurveRegions(id == 1); });
    addGroup(group, groups);
    showCurveRegions(false);
}

QWidget* AdjustPanel::buildCurvePoints(AdjustGroup* group)
{
    for (const QString& key : curveKeys()) {
        group->addKeys({key.toStdString()});
    }
    auto* points = new QWidget(group);
    auto* column = new QVBoxLayout(points);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(kGapSmall);
    auto* channels = new QWidget(points);
    auto* channelRow = new QHBoxLayout(channels);
    channelRow->setContentsMargins(0, 0, 0, 0);
    channelRow->setSpacing(0);
    m_curveChannels = new QButtonGroup(this);
    for (const CurveChannel& c : curveChannels()) {
        auto* b = new QPushButton(tr(c.label), channels);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        b->setChecked(QLatin1StringView(c.key) == m_curveKey);
        b->setProperty("curveKey", QString::fromLatin1(c.key));
        if (c.channel != nullptr) {
            b->setProperty("channel", QString::fromLatin1(c.channel));
        }
        m_curveChannels->addButton(b);
        channelRow->addWidget(b, 1);
    }
    connect(m_curveChannels, &QButtonGroup::buttonClicked, this,
            [this](QAbstractButton* b) { showCurve(b->property("curveKey").toString()); });
    m_curve = new CurveEditor(points);
    auto* hint = new QLabel(tr("Double-click a point to remove it"), points);
    hint->setObjectName(u"faint"_s);
    column->addWidget(channels);
    column->addWidget(m_curve);
    column->addWidget(hint);
    connect(m_curve, &CurveEditor::curveChanged, this, &AdjustPanel::curveEdited);
    connect(m_curve, &CurveEditor::editFinished, this, [this] {
        const auto* channel = m_curveChannels->checkedButton();
        Q_EMIT editFinished(m_curveKey == u"curves.rgb"_s ? tr("Curve")
                                                          : tr("%1 curve").arg(channel->text()));
    });
    m_pointsWidgets.push_back(points);
    return points;
}

void AdjustPanel::buildCurveRegions(AdjustGroup* group)
{
    m_regionCurve = new RegionCurveView(group);
    group->addContent(m_regionCurve);
    m_regionsWidgets.push_back(m_regionCurve);
    for (const char* key : kSplitKeys) {
        group->addKeys({key});
    }
    for (const Region& r : kRegions) {
        AdjustSlider* s = addSlider(group, QString::fromLatin1(r.key), tr(r.label));
        s->setFullName(tr(r.name));
        m_regionsWidgets.push_back(s);
    }
    auto* hint = new QLabel(tr("Drag the marks under the curve to move where the regions meet"), group);
    hint->setObjectName(u"faint"_s);
    hint->setWordWrap(true);
    hint->setContentsMargins(0, 2, 0, 0);
    group->addContent(hint);
    m_regionsWidgets.push_back(hint);
    connect(m_regionCurve, &RegionCurveView::splitMoved, this, &AdjustPanel::splitMoved);
}

void AdjustPanel::showCurveRegions(bool regions)
{
    m_curveModes->button(regions ? 1 : 0)->setChecked(true);
    for (QWidget* w : m_pointsWidgets) {
        w->setVisible(!regions);
    }
    for (QWidget* w : m_regionsWidgets) {
        w->setVisible(regions);
    }
}

bool AdjustPanel::showsCurveRegions() const
{
    return m_curveModes->checkedId() == 1;
}

void AdjustPanel::followCurveMode()
{
    const bool points = std::ranges::any_of(curveKeys(), [this](const QString& key) {
        const pfx::Curve c = m_settings.curve(key.toStdString());
        return c.size() >= 2 && c != pfx::Curve{{0, 0}, {1, 1}};
    });
    const bool regions = std::ranges::any_of(
        kRegions, [this](const Region& r) { return m_settings.number(r.key, 0.0) != 0.0; });
    if (points != regions) {
        showCurveRegions(regions);
    }
}

void AdjustPanel::splitMoved(int index, double value, bool finished)
{
    const auto i = static_cast<std::size_t>(index);
    if (value == kSplitDefaults.at(i)) {
        m_settings.erase(kSplitKeys.at(i));
    } else {
        m_settings.set(kSplitKeys.at(i), value);
    }
    m_regionCurve->setSettings(m_settings);
    refreshGroups();
    Q_EMIT settingsChanged(m_settings);
    if (finished) {
        //: A history step: where two curve regions meet, "Region split 30".
        Q_EMIT editFinished(tr("Region split %1").arg(qRound(value)));
    }
}

void AdjustPanel::showCurve(const QString& key)
{
    m_curveKey = key;
    for (const CurveChannel& c : curveChannels()) {
        if (key == QLatin1StringView(c.key)) {
            m_curve->setColor(channelColor(c.channel));
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

} // namespace pixelfx::ui
