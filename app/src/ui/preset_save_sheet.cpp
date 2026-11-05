#include "ui/preset_save_sheet.h"

#include "core/looks.h"
#include "core/presets.h"
#include "ui/style.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

PresetSaveSheet::PresetSaveSheet(const pfx::Settings& settings, const QStringList& groups, QWidget* parent)
    : Sheet(parent)
    , m_name(new QLineEdit(this))
    , m_group(new QComboBox(this))
{
    setWindowTitle(tr("Save as preset"));
    auto* title = new QLabel(tr("Save as preset"), this);
    title->setObjectName(u"sheetTitle"_s);
    m_name->setPlaceholderText(tr("For example: Warm evening"));
    connect(m_name, &QLineEdit::textChanged, this, &PresetSaveSheet::refreshSave);
    QStringList all = groups;
    const QString mine = core::PresetLibrary::myPresetsGroup();
    all.removeAll(tr("Built in"));
    all.removeAll(mine);
    all.prepend(mine);
    m_group->addItems(all);
    m_group->addItem(tr("New group..."));
    connect(m_group, &QComboBox::activated, this, [this](int index) {
        if (index != m_group->count() - 1) {
            return;
        }
        const QString name = QInputDialog::getText(this, tr("New group"), tr("Group name"));
        if (name.trimmed().isEmpty()) {
            m_group->setCurrentIndex(0);
            return;
        }
        m_group->insertItem(index, name.trimmed());
        m_group->setCurrentIndex(index);
    });

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(16);
    form->addRow(tr("Name"), m_name);
    form->addRow(tr("Group"), m_group);

    buildInclude(form, settings);

    auto* note =
        new QLabel(tr("Settings left out keep whatever each photo already has, so a preset with only "
                      "Color can be stacked on top of another."),
                   this);
    note->setObjectName(u"faint"_s);
    note->setWordWrap(true);

    QHBoxLayout* buttons = buildButtons();

    QVBoxLayout* column = sheetLayout(this, buttons);
    auto* titleRow = new QHBoxLayout;
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(sheetCloseButton(this));
    column->addLayout(titleRow);
    column->addLayout(form);
    column->addWidget(note);
    setMinimumWidth(460);
    tick(m_changed);
    refreshSave();
}

QHBoxLayout* PresetSaveSheet::buildButtons()
{
    auto* toFile = new QPushButton(tr("Save to file..."), this);
    toFile->setToolTip(tr("Also write a .pfxpreset file to share"));
    connect(toFile, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getSaveFileName(
            this, tr("Save preset to file"), name() + u".pfxpreset"_s, tr("PixelFx presets (*.pfxpreset)"));
        if (!file.isEmpty()) {
            m_fileCopy = file;
            accept();
        }
    });
    auto* cancel = new QPushButton(tr("Cancel"), this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    m_save = new QPushButton(tr("Save preset"), this);
    m_save->setObjectName(u"primary"_s);
    m_save->setDefault(true);
    connect(m_save, &QPushButton::clicked, this, &QDialog::accept);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(toFile);
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(m_save);

    return buttons;
}

QLabel* PresetSaveSheet::unitNote(const QString& unit, const pfx::Settings& settings, int changed)
{
    QString text;
    if (unit == u"wb"_s) {
        text = tr("relative to the photo");
    } else if (unit == u"look"_s && !settings.text("look.id", "").empty()) {
        const QString name = core::lookName(QString::fromStdString(settings.text("look.id", "")));
        text = u"%1 %2"_s.arg(name.isEmpty() ? tr("A look") : name)
                   .arg(std::lround(settings.number("look.strength", 100.0)));
    } else if (changed > 0) {
        text = tr("%n changed", nullptr, changed);
    } else {
        return nullptr;
    }
    auto* note = new QLabel(text, this);
    note->setObjectName(u"faint"_s);
    return note;
}

void PresetSaveSheet::buildInclude(QFormLayout* form, const pfx::Settings& settings)
{
    // Include: Changed only, All, None, then one box per part with how much changed.
    const auto link = [this](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(u"link"_s);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };
    auto* changedOnly = link(tr("Changed only"), tr("Tick the parts you changed"));
    auto* everything = link(tr("All"), tr("Tick every part"));
    auto* nothing = link(tr("None"), tr("Untick every part"));
    auto* links = new QHBoxLayout;
    links->setSpacing(4);
    links->addWidget(changedOnly);
    links->addWidget(new QLabel(u"·"_s, this));
    links->addWidget(everything);
    links->addWidget(new QLabel(u"·"_s, this));
    links->addWidget(nothing);
    links->addStretch();
    form->addRow(tr("Include"), links);

    auto* boxes = new QVBoxLayout;
    boxes->setSpacing(4);
    for (const core::PresetUnit& unit : core::presetUnits()) {
        const int changed = core::changedCount(settings, unit.id);
        auto* box = new QCheckBox(unit.label, this);
        auto* row = new QHBoxLayout;
        row->addWidget(box);
        row->addStretch();
        if (QLabel* note = unitNote(unit.id, settings, changed)) {
            row->addWidget(note);
        }
        boxes->addLayout(row);
        m_boxes[unit.id] = box;
        connect(box, &QCheckBox::toggled, this, &PresetSaveSheet::refreshSave);
        if (changed > 0 && unit.id != u"wb"_s && unit.id != u"crop"_s) {
            m_changed << unit.id;
        }
    }
    form->addRow(QString(), boxes);
    connect(changedOnly, &QPushButton::clicked, this, [this] { tick(m_changed); });
    connect(everything, &QPushButton::clicked, this, [this] {
        QStringList ids;
        for (const auto& [id, box] : m_boxes) {
            ids << id;
        }
        tick(ids);
    });
    connect(nothing, &QPushButton::clicked, this, [this] { tick({}); });
}

PresetSaveSheet::~PresetSaveSheet() = default;

QString PresetSaveSheet::name() const
{
    return m_name->text().trimmed();
}

QString PresetSaveSheet::group() const
{
    return m_group->currentText();
}

QStringList PresetSaveSheet::units() const
{
    QStringList out;
    for (const core::PresetUnit& unit : core::presetUnits()) {
        if (m_boxes.at(unit.id)->isChecked()) {
            out << unit.id;
        }
    }
    return out;
}

void PresetSaveSheet::setName(const QString& name)
{
    m_name->setText(name);
}

void PresetSaveSheet::tick(const QStringList& units)
{
    for (const auto& [id, box] : m_boxes) {
        box->setChecked(units.contains(id));
    }
}

void PresetSaveSheet::refreshSave()
{
    // A preset needs a name and something in it.
    m_save->setEnabled(!name().isEmpty() && !units().isEmpty());
}

} // namespace pixelfx::ui
