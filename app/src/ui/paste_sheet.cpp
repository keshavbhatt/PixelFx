#include "ui/paste_sheet.h"

#include "core/presets.h"
#include "ui/part_checklist.h"
#include "ui/style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

PasteSheet::PasteSheet(const pfx::Settings& copied, int photos, QWidget* parent)
    : Sheet(parent)
    , m_parts(new PartChecklist(copied, core::unitsIn(copied), 1, this))
    , m_source(new QLabel(this))
{
    setWindowTitle(tr("Paste settings"));
    m_source->setObjectName(u"muted"_s);
    m_source->hide();
    auto* hint = new QLabel(tr("Parts left out keep what each photo already has."), this);
    hint->setObjectName(u"faint"_s);
    hint->setWordWrap(true);
    const FootButtons buttons = footButtons(this, tr("Paste to %n photos", nullptr, photos), {}, true);
    connect(buttons.cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(buttons.primary, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_parts, &PartChecklist::toggled, this,
            [this, paste = buttons.primary] { paste->setEnabled(!units().isEmpty()); });
    QVBoxLayout* column = sheetLayout(this, buttons.row);
    column->addLayout(sheetTitleRow(this, tr("Paste settings to %n photos", nullptr, photos)));
    column->addWidget(m_source);
    column->addWidget(m_parts);
    column->addWidget(hint);
    setSize(Size::Narrow);
}

PasteSheet::~PasteSheet() = default;

void PasteSheet::setSource(const QString& source)
{
    m_source->setText(tr("From %1").arg(source));
    m_source->setVisible(!source.isEmpty());
}

QStringList PasteSheet::units() const
{
    return m_parts->ticked();
}

void PasteSheet::tick(const QStringList& units)
{
    m_parts->tick(units);
}

} // namespace pixelfx::ui
