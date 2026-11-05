#include "ui/paste_sheet.h"

#include "core/presets.h"
#include "ui/style.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

PasteSheet::PasteSheet(const pfx::Settings& copied, int photos, QWidget* parent)
    : Sheet(parent)
{
    setWindowTitle(tr("Paste settings"));
    auto* title = new QLabel(tr("Paste to %n photos", nullptr, photos), this);
    title->setObjectName(u"sheetTitle"_s);
    auto* hint = new QLabel(tr("Parts left out keep what each photo already has."), this);
    hint->setObjectName(u"faint"_s);
    auto* boxes = new QVBoxLayout;
    boxes->setSpacing(4);
    const QStringList held = core::unitsIn(copied);
    for (const core::PresetUnit& unit : core::presetUnits()) {
        if (!held.contains(unit.id)) {
            continue;
        }
        auto* box = new QCheckBox(unit.label, this);
        box->setChecked(unit.id != u"wb"_s && unit.id != u"crop"_s);
        boxes->addWidget(box);
        m_boxes[unit.id] = box;
    }
    auto* cancel = new QPushButton(tr("Cancel"), this);
    auto* paste = new QPushButton(tr("Paste to %n photos", nullptr, photos), this);
    paste->setObjectName(u"primary"_s);
    paste->setDefault(true);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(paste, &QPushButton::clicked, this, &QDialog::accept);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(paste);
    QVBoxLayout* column = sheetLayout(this, buttons);
    auto* titleRow = new QHBoxLayout;
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(sheetCloseButton(this));
    column->addLayout(titleRow);
    column->addLayout(boxes);
    column->addWidget(hint);
    setMinimumWidth(360);
}

PasteSheet::~PasteSheet() = default;

QStringList PasteSheet::units() const
{
    QStringList out;
    for (const auto& [id, box] : m_boxes) {
        if (box->isChecked()) {
            out << id;
        }
    }
    return out;
}

void PasteSheet::tick(const QStringList& units)
{
    for (const auto& [id, box] : m_boxes) {
        box->setChecked(units.contains(id));
    }
}

} // namespace pixelfx::ui
