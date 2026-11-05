#include "ui/look_editor.h"

#include "services/qt_image_bridge.h"
#include "ui/curve_editor.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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

constexpr int kSliderTicks = 1000;

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

} // namespace

LookEditor::LookEditor(const std::shared_ptr<const pfx::Image>& source, const pfx::Settings& settings,
                       const core::LookEntry& entry, QString folder, QWidget* parent)
    : QDialog(parent)
    , m_path(entry.path)
    , m_folder(std::move(folder))
    , m_photoSettings(core::withLook(settings, {}, 0))
    , m_previewTimer(new QTimer(this))
{
    if (entry.look) {
        m_look = *entry.look;
    } else {
        m_look.category = "color";
        m_look.defaultStrength = 80;
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
    resize(960, 640);

    auto* title = new QLabel(windowTitle(), this);
    title->setObjectName(u"sheetTitle"_s);
    m_name = new QLineEdit(entry.look ? entry.name : tr("My look"), this);
    m_name->setFixedWidth(220);
    m_name->setAccessibleName(tr("Name"));
    m_category = new QComboBox(this);
    for (const Category& c : kCategories) {
        m_category->addItem(tr(c.label), QString::fromLatin1(c.id));
    }
    m_category->setCurrentIndex(std::max(0, m_category->findData(QString::fromStdString(m_look.category))));
    m_category->setAccessibleName(tr("Category"));
    auto* head = new QHBoxLayout;
    head->setContentsMargins(20, 14, 20, 12);
    head->addWidget(title);
    head->addSpacing(8);
    head->addWidget(m_name);
    head->addWidget(m_category);
    head->addStretch();
    head->addWidget(sheetCloseButton(this));

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(buildSteps());
    body->addWidget(buildPreview(), 1);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(head);
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
    QStringList parts;
    for (const pfx::ParamSpec& p : op->params) {
        const pfx::Value* v = step.params.find(p.key);
        if (v == nullptr) {
            continue;
        }
        if (const auto* d = std::get_if<double>(v); d != nullptr && std::abs(*d - p.defaultValue) > 1e-9) {
            parts << u"%1 %2"_s.arg(paramName(*op, p.key)).arg(*d, 0, 'f', decimalsFor(p));
        } else if (std::holds_alternative<pfx::Curve>(*v)) {
            parts << tr("a curve");
        }
    }
    return parts.isEmpty() ? tr("No change yet") : parts.mid(0, 3).join(u", "_s);
}

void LookEditor::showStepMenu(const QPoint& at)
{
    const int row = m_steps->row(m_steps->itemAt(at));
    if (row < 0) {
        return;
    }
    QMenu menu(this);
    const auto move = [this, row](int to) {
        if (to < 0 || std::cmp_greater_equal(to, m_look.steps.size())) {
            return;
        }
        std::swap(m_look.steps[static_cast<std::size_t>(row)], m_look.steps[static_cast<std::size_t>(to)]);
        refreshSteps();
        selectStep(to);
        schedulePreview();
    };
    connect(menu.addAction(tr("Move up")), &QAction::triggered, this, [move, row] { move(row - 1); });
    connect(menu.addAction(tr("Move down")), &QAction::triggered, this, [move, row] { move(row + 1); });
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
    menu.exec(m_steps->viewport()->mapToGlobal(at));
}

QHBoxLayout* LookEditor::buildStepButtons(QWidget* side)
{
    auto* add = new QPushButton(icons::themed(u"plus"_s, tokens().text), tr("Add step"), side);
    add->setObjectName(u"small"_s);
    auto* addMenu = new QMenu(add);
    for (const pfx::OpSpec& op : pfx::registry().ops()) {
        if (op.id != "geometry" && op.id != "heal") { // a look never moves or heals pixels
            const QString id = QString::fromStdString(op.id);
            connect(addMenu->addAction(stepName(id)), &QAction::triggered, this, [this, id] { addStep(id); });
        }
    }
    add->setMenu(addMenu);
    auto* import = new QPushButton(icons::themed(u"import"_s, tokens().text), tr("Import LUT"), side);
    import->setObjectName(u"small"_s);
    connect(import, &QPushButton::clicked, this, &LookEditor::importLut);
    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(10, 8, 10, 8);
    buttons->addWidget(add);
    buttons->addWidget(import);
    buttons->addStretch();
    return buttons;
}

QWidget* LookEditor::buildSteps()
{
    auto* side = new QWidget(this);
    side->setObjectName(u"watermarkControls"_s); // the same panel look as the watermark sheet
    side->setAttribute(Qt::WA_StyledBackground);
    side->setFixedWidth(320);
    auto* label = new QLabel(tr("Steps, applied top to bottom").toUpper(), side);
    label->setObjectName(u"section"_s);
    m_steps = new QListWidget(side);
    m_steps->setObjectName(u"lookSteps"_s);
    m_steps->setDragDropMode(QAbstractItemView::InternalMove);
    m_steps->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_steps->setTextElideMode(Qt::ElideRight);
    m_steps->setWordWrap(false);
    m_steps->setContextMenuPolicy(Qt::CustomContextMenu);
    m_steps->setAccessibleName(tr("Steps"));
    connect(m_steps, &QListWidget::currentRowChanged, this, &LookEditor::showParams);
    connect(m_steps->model(), &QAbstractItemModel::rowsMoved, this, &LookEditor::stepsReordered);
    connect(m_steps, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        const int index = item->data(Qt::UserRole).toInt();
        if (index >= 0 && std::cmp_less(index, m_look.steps.size())) {
            m_look.steps[static_cast<std::size_t>(index)].enabled = item->checkState() == Qt::Checked;
            schedulePreview();
        }
    });
    connect(m_steps, &QListWidget::customContextMenuRequested, this, &LookEditor::showStepMenu);

    QHBoxLayout* buttons = buildStepButtons(side);

    m_paramsTitle = new QLabel(side);
    m_paramsTitle->setObjectName(u"section"_s);
    m_params = new QWidget(side);
    m_paramsLayout = new QVBoxLayout(m_params);
    m_paramsLayout->setContentsMargins(14, 0, 14, 8);
    auto* scroll = new QScrollArea(side);
    scroll->setObjectName(u"adjustScroll"_s);
    scroll->setWidget(m_params);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_params->setObjectName(u"adjustContent"_s);

    auto* column = new QVBoxLayout(side);
    column->setContentsMargins(0, 10, 0, 0);
    column->setSpacing(4);
    column->addWidget(label);
    label->setContentsMargins(14, 0, 14, 0);
    column->addWidget(m_steps, 1);
    column->addLayout(buttons);
    m_paramsTitle->setContentsMargins(14, 4, 14, 0);
    column->addWidget(m_paramsTitle);
    column->addWidget(scroll, 1);
    return side;
}

QWidget* LookEditor::buildPreviewBar(QWidget* side)
{
    auto* bar = new QWidget(side);
    bar->setObjectName(u"watermarkBar"_s);
    bar->setAttribute(Qt::WA_StyledBackground);
    auto* on = new QLabel(tr("Preview on"), bar);
    on->setObjectName(u"faint"_s);
    auto* group = new QButtonGroup(bar);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(0);
    layout->addWidget(on);
    layout->addSpacing(6);
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
    auto* strengthName = new QLabel(tr("Default strength"), bar);
    strengthName->setObjectName(u"adjustName"_s);
    m_strengthValue = new QLabel(bar);
    m_strengthValue->setObjectName(u"adjustValue"_s);
    m_strength = new QSlider(Qt::Horizontal, bar);
    m_strength->setRange(0, 100);
    m_strength->setFixedWidth(130);
    m_strength->setAccessibleName(tr("Default strength"));
    connect(m_strength, &QSlider::valueChanged, this, [this](int v) {
        m_look.defaultStrength = v;
        m_strengthValue->setText(QString::number(v));
        schedulePreview();
    });
    m_strength->setValue(m_look.defaultStrength);
    m_strengthValue->setText(QString::number(m_look.defaultStrength));
    layout->addWidget(strengthName);
    layout->addSpacing(6);
    layout->addWidget(m_strength);
    layout->addSpacing(6);
    layout->addWidget(m_strengthValue);
    return bar;
}

QWidget* LookEditor::buildPreview()
{
    auto* side = new QWidget(this);
    m_preview = new QLabel(side);
    m_preview->setObjectName(u"watermarkPreview"_s);
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
    auto* exportButton = new QPushButton(tr("Export look..."), this);
    connect(exportButton, &QPushButton::clicked, this, &LookEditor::exportLook);
    auto* bake = new QPushButton(tr("Bake to .cube..."), this);
    bake->setToolTip(tr("The colour-only steps as a LUT for other apps"));
    connect(bake, &QPushButton::clicked, this, &LookEditor::bakeCube);
    auto* cancel = new QPushButton(tr("Cancel"), this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* save = new QPushButton(tr("Save look"), this);
    save->setObjectName(u"primary"_s);
    save->setDefault(true);
    connect(save, &QPushButton::clicked, this, &LookEditor::saveLook);
    auto* foot = new QHBoxLayout;
    foot->setContentsMargins(20, 10, 20, 14);
    foot->addWidget(exportButton);
    foot->addWidget(bake);
    foot->addStretch();
    foot->addWidget(cancel);
    foot->addWidget(save);
    column->addWidget(sheetFootBar(this, foot));
}

void LookEditor::refreshSteps()
{
    const QSignalBlocker block(m_steps);
    m_steps->clear();
    for (std::size_t i = 0; i < m_look.steps.size(); ++i) {
        const pfx::LookStep& step = m_look.steps[i];
        auto* item =
            new QListWidgetItem(stepName(QString::fromStdString(step.op)) + u'\n' + stepSummary(step));
        item->setData(Qt::UserRole, static_cast<int>(i));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
        item->setCheckState(step.enabled ? Qt::Checked : Qt::Unchecked);
        item->setToolTip(tr("Drag to reorder; right-click to duplicate or remove"));
        m_steps->addItem(item);
    }
}

void LookEditor::selectStep(int row)
{
    m_steps->setCurrentRow(row);
    showParams(row);
}

void LookEditor::stepsReordered()
{
    // The list's new order of the steps it was built from.
    std::vector<pfx::LookStep> ordered;
    for (int r = 0; r < m_steps->count(); ++r) {
        const int index = m_steps->item(r)->data(Qt::UserRole).toInt();
        ordered.push_back(m_look.steps[static_cast<std::size_t>(index)]);
    }
    m_look.steps = std::move(ordered);
    const int row = m_steps->currentRow();
    refreshSteps();
    selectStep(row);
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
    if (QListWidgetItem* item = m_steps->item(row)) {
        const QSignalBlocker block(m_steps);
        const pfx::LookStep& s = m_look.steps[static_cast<std::size_t>(row)];
        item->setText(stepName(QString::fromStdString(s.op)) + u'\n' + stepSummary(s));
    }
    schedulePreview();
}

void LookEditor::addParamSlider(int row, const QString& name, double low, double high, double value,
                                int decimals, const std::string& key)
{
    auto* label = new QLabel(name, m_params);
    label->setObjectName(u"adjustName"_s);
    auto* shown = new QLabel(QString::number(value, 'f', decimals), m_params);
    shown->setObjectName(u"adjustValue"_s);
    auto* slider = new QSlider(Qt::Horizontal, m_params);
    slider->setRange(0, kSliderTicks);
    slider->setAccessibleName(name);
    slider->setValue(static_cast<int>(std::lround((value - low) / (high - low) * kSliderTicks)));
    connect(slider, &QSlider::valueChanged, this, [this, row, low, high, decimals, key, shown](int t) {
        const double v = low + ((high - low) * t / kSliderTicks);
        m_look.steps[static_cast<std::size_t>(row)].params.set(key, v);
        shown->setText(QString::number(v, 'f', decimals));
        updateStepRow(row);
    });
    auto* top = new QHBoxLayout;
    top->addWidget(label);
    top->addStretch();
    top->addWidget(shown);
    auto* box = new QWidget(m_params);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 2, 0, 2);
    column->setSpacing(0);
    column->addLayout(top);
    column->addWidget(slider);
    m_paramsLayout->addWidget(box);
}

void LookEditor::showParams(int row)
{
    while (const QLayoutItem* item = m_paramsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    if (row < 0 || std::cmp_greater_equal(row, m_look.steps.size())) {
        m_paramsTitle->setText(QString());
        return;
    }
    const pfx::LookStep& step = m_look.steps[static_cast<std::size_t>(row)];
    m_paramsTitle->setText(tr("Step: %1").arg(stepName(QString::fromStdString(step.op))).toUpper());
    if (step.op == "lut") {
        addParamSlider(row, tr("Mix"), 0, 100, step.params.number("mix", 100.0), 0, "mix");
    } else if (const pfx::OpSpec* op = pfx::registry().op(step.op)) {
        for (const pfx::ParamSpec& p : op->params) {
            if (p.type == pfx::ParamType::Number) {
                addParamSlider(row, paramName(*op, p.key), p.minimum, p.maximum,
                               step.params.number(p.key, p.defaultValue), decimalsFor(p), p.key);
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
    pfx::Result<pfx::Lut> lut = pfx::Error{};
    if (file.endsWith(u".cube"_s, Qt::CaseInsensitive)) {
        lut = pfx::loadCube(file.toStdString());
    } else if (auto picture = pfx::load(file.toStdString())) {
        lut = pfx::lutFromHald(picture.value().image);
    }
    if (!lut) {
        m_paramsTitle->setText(tr("That LUT could not be read.").toUpper());
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
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Look;
    // An id of its own, so the file opens as a look wherever it is imported.
    doc.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    doc.name = m_name->text().trimmed().toStdString();
    doc.look = m_look;
    doc.look.category = m_category->currentData().toString().toStdString();
    if (!pfx::saveDocument(doc, target.toStdString()).ok()) {
        m_paramsTitle->setText(tr("The look could not be exported.").toUpper());
    }
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
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QByteArray::fromStdString(pfx::cubeText(baked.lut)));
        (void)file.commit();
    }
    if (!baked.leftOut.empty()) {
        QStringList names;
        for (const std::string& op : baked.leftOut) {
            names << stepName(QString::fromStdString(op));
        }
        m_paramsTitle->setText(tr("Left out of the .cube: %1").arg(names.join(u", "_s)).toUpper());
    }
}

bool LookEditor::saveLook()
{
    const QString name = m_name->text().trimmed();
    if (name.isEmpty()) {
        m_name->setFocus();
        return false;
    }
    m_look.category = m_category->currentData().toString().toStdString();
    QString path;
    m_savedId = core::LookLibrary(m_folder).save(name, m_look, m_path, &path);
    if (m_savedId.isEmpty()) {
        return false;
    }
    m_path = std::move(path);
    accept();
    return true;
}

void LookEditor::schedulePreview()
{
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
