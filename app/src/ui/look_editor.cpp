#include "ui/look_editor.h"

#include "services/qt_image_bridge.h"
#include "ui/curve_editor.h"
#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/message_line.h"
#include "ui/slider_row.h"
#include "ui/step_list.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSlider>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <pfx/color.h>
#include <pfx/document.h>
#include <pfx/io.h>
#include <pfx/lut.h>
#include <pfx/pipeline.h>
#include <pfx/registry.h>
#include <pfx/resample.h>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kVisibleSteps = 4; // rows shown before the list scrolls
constexpr int kNewLookStrength = 80;

struct Category
{
    const char* id;
    const char* label;
};
constexpr std::array<Category, 3> kCategories = {{
    {"color", QT_TRANSLATE_NOOP("LookEditor", "Color")},
    {"film", QT_TRANSLATE_NOOP("LookEditor", "Film")},
    {"bw", QT_TRANSLATE_NOOP("LookEditor", "B&W")},
}};

// A ramp of every hue at full and half saturation over a grey scale, for checking a look on
// colours a photo may not have.
std::shared_ptr<const pfx::Image> makeRamp()
{
    constexpr int kWidth = 600;
    constexpr int kHeight = 400;
    auto ramp = std::make_shared<pfx::Image>(kWidth, kHeight);
    const pfx::color::Mat3& m = pfx::color::kSrgbToRec2020;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const double t = static_cast<double>(x) / (kWidth - 1);
            QColor c;
            if (y < kHeight * 3 / 4) {
                const double v = 1.0 - (0.6 * y / (kHeight * 3.0 / 4.0));
                c = QColor::fromHsvF(static_cast<float>(t * 0.999), 1.0F - static_cast<float>(y) / kHeight,
                                     static_cast<float>(v));
            } else {
                c = QColor::fromRgbF(static_cast<float>(t), static_cast<float>(t), static_cast<float>(t));
            }
            const std::array<double, 3> lin = {pfx::color::srgbDecode(static_cast<double>(c.redF())),
                                               pfx::color::srgbDecode(static_cast<double>(c.greenF())),
                                               pfx::color::srgbDecode(static_cast<double>(c.blueF()))};
            float* p = ramp->pixel(x, y);
            for (std::size_t i = 0; i < 3; ++i) {
                p[i] =
                    static_cast<float>((m.at(i)[0] * lin[0]) + (m.at(i)[1] * lin[1]) + (m.at(i)[2] * lin[2]));
            }
            p[3] = 1.0F;
        }
    }
    return ramp;
}

// "Shadows hue" from "grading.shadows.hue", "Amount" from "effects.vignette.amount".
QString paramName(const pfx::OpSpec& op, const std::string& key)
{
    QStringList parts = QString::fromStdString(key).split(u'.');
    if (!parts.isEmpty() && parts.front() == QString::fromStdString(op.group)) {
        parts.removeFirst();
    }
    if (parts.size() > 1 && parts.front() == QString::fromStdString(op.id)) {
        parts.removeFirst();
    }
    QString name = parts.join(u' ');
    if (!name.isEmpty()) {
        name[0] = name[0].toUpper();
    }
    return name;
}

int decimalsFor(const pfx::ParamSpec& p)
{
    const double range = p.maximum - p.minimum;
    if (range <= 0.01) {
        return 4;
    }
    return range <= 10.0 ? 2 : 0;
}

// A value as the panel shows it: signed when it can go both ways, with its unit ("+10", "200°").
QString shownValue(double value, int decimals, double minimum, const std::string& unit)
{
    QString text = QString::number(value, 'f', decimals);
    if (minimum < 0.0 && value > 0.0) {
        text.prepend(u'+');
    }
    if (unit == "degrees") {
        text += u'\u00B0';
    } else if (unit == "EV") {
        text += LookEditor::tr(" EV");
    }
    return text;
}

// The colour a hue reads as, for summaries ("teal").
QString hueName(double degrees)
{
    struct Band
    {
        double upTo;
        const char* name;
    };
    static constexpr std::array<Band, 9> kBands = {{
        {15, QT_TRANSLATE_NOOP("LookEditor", "red")},
        {45, QT_TRANSLATE_NOOP("LookEditor", "orange")},
        {70, QT_TRANSLATE_NOOP("LookEditor", "yellow")},
        {160, QT_TRANSLATE_NOOP("LookEditor", "green")},
        {210, QT_TRANSLATE_NOOP("LookEditor", "teal")},
        {255, QT_TRANSLATE_NOOP("LookEditor", "blue")},
        {290, QT_TRANSLATE_NOOP("LookEditor", "purple")},
        {340, QT_TRANSLATE_NOOP("LookEditor", "magenta")},
        {361, QT_TRANSLATE_NOOP("LookEditor", "red")},
    }};
    for (const Band& b : kBands) {
        if (degrees < b.upTo) {
            return LookEditor::tr(b.name);
        }
    }
    return LookEditor::tr("red");
}

// The curve's height at `x`, between its points.
double curveAt(const pfx::Curve& curve, double x)
{
    if (curve.empty()) {
        return x;
    }
    if (x <= curve.front().x) {
        return curve.front().y;
    }
    for (std::size_t i = 1; i < curve.size(); ++i) {
        const pfx::CurvePoint& a = curve[i - 1];
        const pfx::CurvePoint& b = curve[i];
        if (x <= b.x) {
            return b.x > a.x ? a.y + ((b.y - a.y) * (x - a.x) / (b.x - a.x)) : b.y;
        }
    }
    return curve.back().y;
}

// A curve in the words photographers use: "S curve, lifted blacks".
QString curveWords(const pfx::Curve& curve)
{
    constexpr double kNoticeable = 0.02;
    const double low = curveAt(curve, 0.25) - 0.25;
    const double high = curveAt(curve, 0.75) - 0.75;
    QStringList words;
    if (low < -kNoticeable && high > kNoticeable) {
        words << LookEditor::tr("S curve");
    } else if (low > kNoticeable && high < -kNoticeable) {
        words << LookEditor::tr("flatter contrast");
    } else if (low > kNoticeable || high > kNoticeable) {
        words << LookEditor::tr("brighter");
    } else if (low < -kNoticeable || high < -kNoticeable) {
        words << LookEditor::tr("darker");
    }
    if (curveAt(curve, 0.0) > kNoticeable) {
        words << LookEditor::tr("lifted blacks");
    }
    if (curveAt(curve, 1.0) < 1.0 - kNoticeable) {
        words << LookEditor::tr("softer whites");
    }
    if (words.isEmpty()) {
        return LookEditor::tr("Straight curve");
    }
    words.front()[0] = words.front().at(0).toUpper();
    return words.join(u", "_s);
}

// The icon of a step, the same as its group in the adjust panel.
QString stepGlyph(const std::string& op)
{
    static const std::array<std::pair<const char*, const char*>, 15> glyphs = {{
        {"denoise", "detail"},
        {"white_balance", "droplet"},
        {"exposure", "sun"},
        {"dehaze", "detail"},
        {"tone", "sun"},
        {"clarity", "detail"},
        {"curves", "curves"},
        {"mixer", "palette"},
        {"vibrance_saturation", "droplet"},
        {"grading", "blend"},
        {"sharpen", "detail"},
        {"vignette", "sparkles"},
        {"grain", "sparkles"},
        {"fade", "sparkles"},
        {"lut", "image"},
    }};
    for (const auto& [id, glyph] : glyphs) {
        if (op == id) {
            return QString::fromLatin1(glyph);
        }
    }
    return u"sliders"_s;
}

} // namespace

LookEditor::LookEditor(const std::shared_ptr<const pfx::Image>& source, const pfx::Settings& settings,
                       const core::LookEntry& entry, QString folder, QWidget* parent)
    : Sheet(parent)
    , m_path(entry.path)
    , m_folder(std::move(folder))
    , m_photoSettings(core::withLook(settings, {}, 0))
    , m_previewTimer(new QTimer(this))
{
    if (entry.look) {
        m_look = *entry.look;
    } else {
        m_look.category = "color";
        m_look.defaultStrength = kNewLookStrength;
    }
    // The preview at a fixed, modest size: quick to render while sliders move.
    if (source) {
        m_photo = source;
        if (auto small = pfx::fitLongEdge(*source, 900)) {
            m_photo = std::make_shared<pfx::Image>(std::move(small.value()));
        }
    }
    setWindowTitle(entry.look ? tr("Edit look") : tr("New look"));
    setModal(true);
    setSize(Size::Editor);

    m_name = new QLineEdit(entry.look ? entry.name : tr("My look"), this);
    m_name->setFixedWidth(220);
    m_name->setAccessibleName(tr("Name"));
    connect(m_name, &QLineEdit::textEdited, this, &LookEditor::schedulePreview);
    m_category = new QComboBox(this);
    for (const Category& c : kCategories) {
        m_category->addItem(tr(c.label), QString::fromLatin1(c.id));
    }
    m_category->setCurrentIndex(std::max(0, m_category->findData(QString::fromStdString(m_look.category))));
    m_category->setAccessibleName(tr("Category"));
    QWidget* head = sheetHeaderBar(this, sheetTitleRow(this, windowTitle(), {m_name, m_category}));

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(buildSteps());
    body->addWidget(buildPreview(), 1);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(head);
    column->addLayout(body, 1);
    buildFooter(column);

    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(60);
    connect(m_previewTimer, &QTimer::timeout, this, &LookEditor::renderPreview);
    refreshSteps();
    selectStep(m_look.steps.empty() ? -1 : 0);
    schedulePreview();
}

LookEditor::~LookEditor() = default;

QString LookEditor::stepName(const QString& op)
{
    static const std::array<std::pair<const char*, const char*>, 15> names = {{
        {"denoise", QT_TRANSLATE_NOOP("LookEditor", "Noise reduction")},
        {"white_balance", QT_TRANSLATE_NOOP("LookEditor", "White balance")},
        {"exposure", QT_TRANSLATE_NOOP("LookEditor", "Exposure")},
        {"dehaze", QT_TRANSLATE_NOOP("LookEditor", "Dehaze")},
        {"tone", QT_TRANSLATE_NOOP("LookEditor", "Tone")},
        {"clarity", QT_TRANSLATE_NOOP("LookEditor", "Clarity")},
        {"curves", QT_TRANSLATE_NOOP("LookEditor", "Tone curve")},
        {"mixer", QT_TRANSLATE_NOOP("LookEditor", "Color mixer")},
        {"vibrance_saturation", QT_TRANSLATE_NOOP("LookEditor", "Vibrance and saturation")},
        {"grading", QT_TRANSLATE_NOOP("LookEditor", "Split toning")},
        {"sharpen", QT_TRANSLATE_NOOP("LookEditor", "Sharpen")},
        {"vignette", QT_TRANSLATE_NOOP("LookEditor", "Vignette")},
        {"grain", QT_TRANSLATE_NOOP("LookEditor", "Grain")},
        {"fade", QT_TRANSLATE_NOOP("LookEditor", "Fade")},
        {"lut", QT_TRANSLATE_NOOP("LookEditor", "LUT")},
    }};
    for (const auto& [id, label] : names) {
        if (op == QLatin1StringView(id)) {
            return tr(label);
        }
    }
    return op;
}

QString LookEditor::stepSummary(const pfx::LookStep& step) const
{
    if (step.op == "lut") {
        const QString resource = QString::fromStdString(step.params.text("resource", ""));
        const auto it = m_look.resources.find(resource.toStdString());
        const QString name = it != m_look.resources.end() && !it->second.title.empty()
                                 ? QString::fromStdString(it->second.title)
                                 : resource;
        return u"%1, %2%"_s.arg(name).arg(std::lround(step.params.number("mix", 100.0)));
    }
    const pfx::OpSpec* op = pfx::registry().op(step.op);
    if (op == nullptr) {
        return tr("Not known to this version: skipped");
    }
    if (step.op == "grading") {
        return gradingSummary(step.params);
    }
    QStringList parts;
    for (const pfx::ParamSpec& p : op->params) {
        const pfx::Value* v = step.params.find(p.key);
        if (v == nullptr) {
            continue;
        }
        if (const auto* d = std::get_if<double>(v); d != nullptr && std::abs(*d - p.defaultValue) > 1e-9) {
            parts << u"%1 %2"_s.arg(paramName(*op, p.key), shownValue(*d, decimalsFor(p), p.minimum, p.unit));
        } else if (const auto* curve = std::get_if<pfx::Curve>(v)) {
            parts << curveWords(*curve);
        }
    }
    return parts.isEmpty() ? tr("No change yet") : parts.mid(0, 3).join(u", "_s);
}

QString LookEditor::gradingSummary(const pfx::Settings& params)
{
    // "Shadows teal 200°, highlights orange 35°"
    QStringList parts;
    const std::array<std::pair<const char*, const char*>, 2> zones = {{
        {"shadows", QT_TRANSLATE_NOOP("LookEditor", "shadows")},
        {"highlights", QT_TRANSLATE_NOOP("LookEditor", "highlights")},
    }};
    for (const auto& [zone, name] : zones) {
        const std::string key = std::string("grading.") + zone;
        if (params.number(key + ".amount", 0.0) <= 0.0) {
            continue;
        }
        const double hue = params.number(key + ".hue", 0.0);
        parts << u"%1 %2 %3\u00B0"_s.arg(tr(name), hueName(hue)).arg(std::lround(hue));
    }
    if (const double balance = params.number("grading.balance", 0.0); std::abs(balance) > 1e-9) {
        parts << tr("balance %1").arg(shownValue(balance, 0, -1.0, {}));
    }
    if (parts.isEmpty()) {
        return tr("No change yet");
    }
    parts.front()[0] = parts.front().at(0).toUpper();
    return parts.join(u", "_s);
}

void LookEditor::showStepMenu(int row, const QPoint& at)
{
    if (row < 0) {
        return;
    }
    QMenu menu(this);
    connect(menu.addAction(tr("Move up")), &QAction::triggered, this,
            [this, row] { m_steps->moveRow(row, row - 1); });
    connect(menu.addAction(tr("Move down")), &QAction::triggered, this,
            [this, row] { m_steps->moveRow(row, row + 1); });
    connect(menu.addAction(tr("Duplicate")), &QAction::triggered, this, [this, row] {
        const pfx::LookStep copy = m_look.steps[static_cast<std::size_t>(row)];
        m_look.steps.insert(m_look.steps.begin() + row + 1, copy);
        refreshSteps();
        selectStep(row + 1);
        schedulePreview();
    });
    connect(menu.addAction(tr("Remove")), &QAction::triggered, this, [this, row] {
        m_look.steps.erase(m_look.steps.begin() + row);
        refreshSteps();
        selectStep(std::min(row, static_cast<int>(m_look.steps.size()) - 1));
        schedulePreview();
    });
    menu.exec(at);
}

QHBoxLayout* LookEditor::buildStepButtons(QWidget* side)
{
    auto* add = new IconTextButton(icons::themed(u"plus"_s, tokens().text), tr("Add step"), side);
    add->setObjectName(u"small"_s);
    auto* addMenu = new QMenu(add);
    for (const pfx::OpSpec& op : pfx::registry().ops()) {
        if (op.id != "geometry" && op.id != "heal") { // a look never moves or heals pixels
            const QString id = QString::fromStdString(op.id);
            connect(addMenu->addAction(stepName(id)), &QAction::triggered, this, [this, id] { addStep(id); });
        }
    }
    add->setMenu(addMenu);
    auto* import = new IconTextButton(icons::themed(u"import"_s, tokens().text), tr("Import LUT"), side);
    import->setObjectName(u"small"_s);
    connect(import, &QPushButton::clicked, this, &LookEditor::importLut);
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(kGapControls);
    buttons->setContentsMargins(kToolRowPadding, 8, kToolRowPadding, 8);
    buttons->addWidget(add);
    buttons->addWidget(import);
    buttons->addStretch();
    return buttons;
}

QWidget* LookEditor::buildSteps()
{
    auto* side = new QWidget(this);
    side->setObjectName(u"sheetSide"_s); // the same panel look as the watermark sheet
    side->setAttribute(Qt::WA_StyledBackground);
    side->setFixedWidth(320);
    QLabel* label = sectionHeading(tr("Steps, applied top to bottom"), side, kPanelPadding);
    m_steps = new StepList(side);
    m_steps->setObjectName(u"panelContent"_s);
    m_steps->setAttribute(Qt::WA_StyledBackground);
    m_steps->setAccessibleName(tr("Steps"));
    connect(m_steps, &StepList::currentChanged, this, &LookEditor::showParams);
    connect(m_steps, &StepList::moved, this, &LookEditor::moveStep);
    connect(m_steps, &StepList::toggled, this, [this](int row, bool on) {
        if (row >= 0 && std::cmp_less(row, m_look.steps.size())) {
            m_look.steps[static_cast<std::size_t>(row)].enabled = on;
            schedulePreview();
        }
    });
    connect(m_steps, &StepList::menuRequested, this, &LookEditor::showStepMenu);
    m_stepsScroll = new QScrollArea(side);
    m_stepsScroll->setObjectName(u"panelScroll"_s);
    m_stepsScroll->setWidget(m_steps);
    m_stepsScroll->setWidgetResizable(true);
    m_stepsScroll->setFrameShape(QFrame::NoFrame);
    m_stepsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_noSteps = new QLabel(tr("No steps yet. Add a step, or import a LUT to start from."), side);
    m_noSteps->setObjectName(u"faint"_s);
    m_noSteps->setWordWrap(true);
    m_noSteps->setContentsMargins(kPanelPadding, 6, kPanelPadding, 6);

    QHBoxLayout* buttons = buildStepButtons(side);

    m_paramsTitle = sectionHeading(QString(), side, kPanelPadding);
    m_params = new QWidget(side);
    m_params->setObjectName(u"panelContent"_s);
    m_paramsLayout = new QVBoxLayout(m_params);
    m_paramsLayout->setContentsMargins(kPanelPadding, 0, kPanelPadding, 8);
    auto* scroll = new QScrollArea(side);
    scroll->setObjectName(u"panelScroll"_s);
    scroll->setWidget(m_params);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* column = new QVBoxLayout(side);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(label);
    column->addWidget(m_noSteps);
    column->addWidget(m_stepsScroll);
    column->addLayout(buttons);
    column->addWidget(m_paramsTitle);
    column->addWidget(scroll, 1);
    return side;
}

QWidget* LookEditor::buildPreviewBar(QWidget* side)
{
    auto* bar = new QWidget(side);
    bar->setObjectName(u"previewBar"_s);
    bar->setAttribute(Qt::WA_StyledBackground);
    auto* on = new QLabel(tr("Preview on"), bar);
    on->setObjectName(u"faint"_s);
    auto* group = new QButtonGroup(bar);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(kPanelPadding, 8, kPanelPadding, 8);
    layout->setSpacing(0);
    layout->addWidget(on);
    layout->addSpacing(kGapRelated);
    const std::array<QString, 2> names = {tr("This photo"), tr("Ramp")};
    for (int i = 0; i < 2; ++i) {
        auto* b = new QPushButton(names.at(static_cast<std::size_t>(i)), bar);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        b->setChecked(i == 0);
        b->setEnabled(i == 1 || m_photo != nullptr);
        group->addButton(b, i);
        layout->addWidget(b);
    }
    if (!m_photo) {
        m_onRamp = true;
        group->button(1)->setChecked(true);
    }
    connect(group, &QButtonGroup::idClicked, this, [this](int id) {
        m_onRamp = id == 1;
        schedulePreview();
    });
    layout->addStretch();
    m_strength = new SliderRow(tr("Default strength"), bar);
    m_strength->setRange(0, 100, kNewLookStrength);
    m_strength->setTicks(m_look.defaultStrength);
    m_strength->setFixedWidth(180);
    m_strength->setToolTip(
        tr("How strongly the look applies when it is chosen; the strength slider changes it per photo"));
    connect(m_strength, &SliderRow::ticksMoved, this, [this](int v) {
        m_look.defaultStrength = v;
        schedulePreview();
    });
    layout->addWidget(m_strength);
    return bar;
}

QWidget* LookEditor::buildPreview()
{
    auto* side = new QWidget(this);
    m_preview = new QLabel(side);
    m_preview->setObjectName(u"previewArea"_s);
    m_preview->setAttribute(Qt::WA_StyledBackground);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(360, 300);
    m_preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    QWidget* bar = buildPreviewBar(side);

    auto* column = new QVBoxLayout(side);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(m_preview, 1);
    column->addWidget(bar);
    return side;
}

void LookEditor::buildFooter(QVBoxLayout* column)
{
    m_export = new IconTextButton(icons::themed(u"share"_s, tokens().text), tr("Export look..."), this);
    connect(m_export, &QPushButton::clicked, this, &LookEditor::exportLook);
    m_bake = new QPushButton(tr("Bake to .cube..."), this);
    connect(m_bake, &QPushButton::clicked, this, &LookEditor::bakeCube);
    m_message = new MessageLine(this);
    const FootButtons buttons = footButtons(this, tr("Save look"), {m_export, m_bake, m_message}, true);
    connect(buttons.cancel, &QPushButton::clicked, this, &QDialog::reject);
    m_save = buttons.primary;
    connect(m_save, &QPushButton::clicked, this, &LookEditor::saveLook);
    column->addWidget(footBar(this, buttons.row));
}

void LookEditor::refreshSteps()
{
    QList<StepList::Row> rows;
    for (const pfx::LookStep& step : m_look.steps) {
        rows.append(
            {stepGlyph(step.op), stepName(QString::fromStdString(step.op)), stepSummary(step), step.enabled});
    }
    m_steps->setRows(rows);
    // A look with no steps changes nothing: there is nothing to save, export or bake yet.
    const bool empty = m_look.steps.empty();
    m_noSteps->setVisible(empty);
    m_stepsScroll->setVisible(!empty);
    for (QPushButton* b : {m_save, m_export, m_bake}) {
        b->setEnabled(!empty);
    }
    m_save->setToolTip(empty ? tr("Add a step first") : QString());
    m_export->setToolTip(empty ? tr("Add a step first") : tr("Save the look as a .pfxlook file to share"));
    m_bake->setToolTip(empty ? tr("Add a step first") : tr("The colour-only steps as a LUT for other apps"));
    m_stepsScroll->setFixedHeight(m_steps->heightForRows(kVisibleSteps));
}

void LookEditor::selectStep(int row)
{
    m_steps->setCurrent(row);
}

void LookEditor::moveStep(int from, int to)
{
    // The list has moved its row already; the steps follow, and the parameters shown point at
    // the new places.
    pfx::LookStep step = m_look.steps[static_cast<std::size_t>(from)];
    m_look.steps.erase(m_look.steps.begin() + from);
    m_look.steps.insert(m_look.steps.begin() + to, std::move(step));
    showParams(m_steps->current());
    schedulePreview();
}

void LookEditor::addStep(const QString& op)
{
    pfx::LookStep step;
    step.op = op.toStdString();
    if (const pfx::OpSpec* spec = pfx::registry().op(step.op)) {
        for (const pfx::ParamSpec& p : spec->params) {
            if (p.type == pfx::ParamType::Number) {
                step.params.set(p.key, p.defaultValue);
            }
        }
    }
    m_look.steps.push_back(std::move(step));
    refreshSteps();
    selectStep(static_cast<int>(m_look.steps.size()) - 1);
    schedulePreview();
}

void LookEditor::updateStepRow(int row)
{
    if (row >= 0 && std::cmp_less(row, m_look.steps.size())) {
        m_steps->setSubtitle(row, stepSummary(m_look.steps[static_cast<std::size_t>(row)]));
    }
    schedulePreview();
}

void LookEditor::addParamSlider(int row, const QString& name, const pfx::ParamSpec& spec, double value)
{
    // Whole ticks of the shown precision: hundredths for small ranges, whole numbers otherwise.
    const int decimals = decimalsFor(spec);
    const double scale = std::pow(10.0, decimals);
    const auto ticks = [scale](double v) { return static_cast<int>(std::lround(v * scale)); };
    auto* slider = new SliderRow(name, m_params);
    slider->setRange(ticks(spec.minimum), ticks(spec.maximum), ticks(spec.defaultValue));
    slider->setTicksPerUnit(scale);
    slider->setFormat(
        [scale, decimals, spec](int t) { return shownValue(t / scale, decimals, spec.minimum, spec.unit); });
    slider->setTicks(ticks(value));
    const std::string key = spec.key;
    connect(slider, &SliderRow::ticksMoved, this, [this, row, scale, key](int t) {
        m_look.steps[static_cast<std::size_t>(row)].params.set(key, t / scale);
        updateStepRow(row);
    });
    m_paramsLayout->addWidget(slider);
}

void LookEditor::showParams(int row)
{
    while (const QLayoutItem* item = m_paramsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    if (row < 0 || std::cmp_greater_equal(row, m_look.steps.size())) {
        m_paramsTitle->hide();
        return;
    }
    const pfx::LookStep& step = m_look.steps[static_cast<std::size_t>(row)];
    m_paramsTitle->setText(tr("Step: %1").arg(stepName(QString::fromStdString(step.op))).toUpper());
    m_paramsTitle->show();
    if (step.op == "lut") {
        pfx::ParamSpec mix;
        mix.key = "mix";
        mix.maximum = 100.0;
        mix.defaultValue = 100.0;
        addParamSlider(row, tr("Mix"), mix, step.params.number("mix", 100.0));
    } else if (const pfx::OpSpec* op = pfx::registry().op(step.op)) {
        for (const pfx::ParamSpec& p : op->params) {
            if (p.type == pfx::ParamType::Number) {
                addParamSlider(row, paramName(*op, p.key), p, step.params.number(p.key, p.defaultValue));
            } else if (p.type == pfx::ParamType::Points && p.key == "curves.rgb") {
                auto* curve = new CurveEditor(m_params);
                curve->setCurve(step.params.curve(p.key));
                connect(curve, &CurveEditor::curveChanged, this, [this, row](const pfx::Curve& c) {
                    m_look.steps[static_cast<std::size_t>(row)].params.set("curves.rgb", c);
                    updateStepRow(row);
                });
                m_paramsLayout->addWidget(curve);
            }
        }
    }
    m_paramsLayout->addStretch();
}

void LookEditor::importLut()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Import a LUT"), QString(),
                                                      tr("LUTs (*.cube *.png *.tif *.tiff)"));
    if (file.isEmpty()) {
        return;
    }
    m_message->clear();
    pfx::Result<pfx::Lut> lut = pfx::Error{};
    if (file.endsWith(u".cube"_s, Qt::CaseInsensitive)) {
        lut = pfx::loadCube(file.toStdString());
    } else if (auto picture = pfx::load(file.toStdString())) {
        lut = pfx::lutFromHald(picture.value().image);
    }
    if (!lut) {
        m_message->showError(tr("%1 could not be read as a LUT.").arg(QFileInfo(file).fileName()));
        return;
    }
    // A resource name of its own, the file's name as the title.
    std::string key = QFileInfo(file).completeBaseName().toStdString();
    for (int n = 2; m_look.resources.contains(key); ++n) {
        key = QFileInfo(file).completeBaseName().toStdString() + "-" + std::to_string(n);
    }
    pfx::Lut table = std::move(lut).value();
    table.title = QFileInfo(file).fileName().toStdString();
    m_look.resources.emplace(key, std::move(table));
    pfx::LookStep step;
    step.op = "lut";
    step.params.set("resource", key);
    step.params.set("mix", 100.0);
    m_look.steps.push_back(std::move(step));
    refreshSteps();
    selectStep(static_cast<int>(m_look.steps.size()) - 1);
    schedulePreview();
}

void LookEditor::exportLook()
{
    const QString target = QFileDialog::getSaveFileName(
        this, tr("Export look"), m_name->text() + u".pfxlook"_s, tr("PixelFx looks (*.pfxlook)"));
    if (target.isEmpty()) {
        return;
    }
    m_message->clear();
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Look;
    // An id of its own, so the file opens as a look wherever it is imported.
    doc.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    doc.name = m_name->text().trimmed().toStdString();
    doc.look = m_look;
    doc.look.category = m_category->currentData().toString().toStdString();
    if (!pfx::saveDocument(doc, target.toStdString()).ok()) {
        m_message->showError(tr("The look could not be written to %1.").arg(QFileInfo(target).fileName()));
        return;
    }
    m_message->showInfo(tr("Exported as %1").arg(QFileInfo(target).fileName()));
}

void LookEditor::bakeCube()
{
    const QString target = QFileDialog::getSaveFileName(
        this, tr("Bake to .cube"), m_name->text() + u".cube"_s, tr("Cube LUTs (*.cube)"));
    if (target.isEmpty()) {
        return;
    }
    pfx::BakedLook baked = pfx::bakeLook(m_look);
    baked.lut.title = m_name->text().trimmed().toStdString();
    QSaveFile file(target);
    const bool written = file.open(QIODevice::WriteOnly) &&
                         file.write(QByteArray::fromStdString(pfx::cubeText(baked.lut))) >= 0 &&
                         file.commit();
    if (!written) {
        m_message->showError(tr("The .cube could not be written to %1.").arg(QFileInfo(target).fileName()));
        return;
    }
    // A LUT holds colour only: steps that look at neighbouring pixels cannot go in it.
    QStringList names;
    for (const std::string& op : baked.leftOut) {
        names << stepName(QString::fromStdString(op));
    }
    m_message->showInfo(names.isEmpty() ? tr("Baked to %1").arg(QFileInfo(target).fileName())
                                        : tr("Baked to %1, without %2 (a LUT holds colour only)")
                                              .arg(QFileInfo(target).fileName(), names.join(u", "_s)));
}

bool LookEditor::saveLook()
{
    const QString name = m_name->text().trimmed();
    if (name.isEmpty()) {
        m_message->showError(tr("Name the look first."));
        m_name->setFocus();
        return false;
    }
    m_look.category = m_category->currentData().toString().toStdString();
    QString path;
    m_savedId = core::LookLibrary(m_folder).save(name, m_look, m_path, &path);
    if (m_savedId.isEmpty()) {
        m_message->showError(
            tr("The look could not be saved in %1.").arg(QDir::toNativeSeparators(m_folder)));
        return false;
    }
    m_path = std::move(path);
    accept();
    return true;
}

void LookEditor::schedulePreview()
{
    if (m_message != nullptr && m_message->isError()) {
        m_message->clear(); // the edit may have put it right
    }
    m_previewTimer->start();
}

void LookEditor::renderPreview()
{
    if (m_rendering) {
        m_pending = true; // one at a time; the newest settings follow
        return;
    }
    if (m_onRamp && !m_ramp) {
        m_ramp = makeRamp();
    }
    const std::shared_ptr<const pfx::Image> source = m_onRamp ? m_ramp : m_photo;
    if (!source) {
        return;
    }
    m_rendering = true;
    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher] {
        m_previewImage = watcher->result();
        watcher->deleteLater();
        m_rendering = false;
        if (!m_previewImage.isNull()) {
            m_preview->setPixmap(QPixmap::fromImage(m_previewImage)
                                     .scaled(m_preview->size() - QSize(32, 32), Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
        }
        Q_EMIT previewRendered();
        if (m_pending) {
            m_pending = false;
            renderPreview();
        }
    });
    const pfx::Settings settings = m_onRamp ? pfx::Settings{} : m_photoSettings;
    watcher->setFuture(QtConcurrent::run([source, settings, look = m_look] {
        pfx::RenderOptions options;
        options.look = &look;
        options.lookStrength = static_cast<float>(look.defaultStrength) / 100.0F;
        const auto rendered = pfx::render(*source, settings, options);
        const auto pixels =
            rendered ? pfx::toSrgb8(rendered.value()) : pfx::Result<pfx::Rgba8Image>(pfx::Error{});
        return pixels ? services::toQImage(pixels.value()) : QImage();
    }));
}

} // namespace pixelfx::ui
