#include "ui/preset_save_sheet.h"

#include "core/presets.h"
#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/part_checklist.h"
#include "ui/style.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>


using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {
constexpr int kWidth = 560;
constexpr int kLabelColumn = 100;
constexpr QSize kThumbnail(150, 100);
} // namespace

PresetSaveSheet::PresetSaveSheet(const pfx::Settings& settings, const QStringList& groups, QWidget* parent)
    : Sheet(parent)
    , m_name(new QLineEdit(this))
    , m_group(new QComboBox(this))
    , m_thumbnail(new QLabel(this))
{
    setWindowTitle(tr("Save as preset"));
    auto* title = new QLabel(tr("Save as preset"), this);
    title->setObjectName(u"sheetTitle"_s);
    QHBoxLayout* top = buildTop(groups);

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
    column->addLayout(top);
    column->addSpacing(2);
    buildInclude(column, settings);
    column->addWidget(note);
    setFixedWidth(kWidth);
    refreshSave();
}

QHBoxLayout* PresetSaveSheet::buildTop(const QStringList& groups)
{
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
    form->setVerticalSpacing(20);
    form->addRow(tr("Name"), m_name);
    form->addRow(tr("Group"), m_group);
    setLabelColumn(form, kLabelColumn);
    m_thumbnail->setFixedSize(kThumbnail);
    m_thumbnail->hide();
    auto* top = new QHBoxLayout;
    top->setSpacing(14);
    top->addWidget(m_thumbnail, 0, Qt::AlignTop);
    top->addLayout(form, 1);
    top->setAlignment(form, Qt::AlignVCenter);

    return top;
}

QHBoxLayout* PresetSaveSheet::buildButtons()
{
    auto* toFile = new IconTextButton(icons::themed(u"share"_s, tokens().text), tr("Save to file..."), this);
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

void PresetSaveSheet::buildInclude(QVBoxLayout* column, const pfx::Settings& settings)
{
    // Include: Changed only, All, None, then the parts.
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
    auto* heading = new QLabel(tr("Include"), this);
    heading->setObjectName(u"heading"_s);
    auto* links = new QHBoxLayout;
    links->setSpacing(4);
    links->addWidget(heading);
    links->addStretch();
    links->addWidget(changedOnly);
    links->addWidget(new QLabel(u"·"_s, this));
    links->addWidget(everything);
    links->addWidget(new QLabel(u"·"_s, this));
    links->addWidget(nothing);
    column->addLayout(links);
    QStringList all;
    for (const core::PresetUnit& unit : core::presetUnits()) {
        all << unit.id;
    }
    m_parts = new PartChecklist(settings, all, 2, this);
    connect(m_parts, &PartChecklist::toggled, this, &PresetSaveSheet::refreshSave);
    column->addWidget(m_parts);
    connect(changedOnly, &QPushButton::clicked, this, [this] { tick(m_parts->changed()); });
    connect(everything, &QPushButton::clicked, this, [this] { tick(m_parts->parts()); });
    connect(nothing, &QPushButton::clicked, this, [this] { tick({}); });
}

PresetSaveSheet::~PresetSaveSheet() = default;

void PresetSaveSheet::setPhoto(const QImage& photo)
{
    m_thumbnail->setPixmap(coverThumbnail(photo, kThumbnail, 8, devicePixelRatioF()));
    m_thumbnail->setVisible(!photo.isNull());
}

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
    return m_parts->ticked();
}

void PresetSaveSheet::setName(const QString& name)
{
    m_name->setText(name);
}

void PresetSaveSheet::tick(const QStringList& units)
{
    m_parts->tick(units);
}

void PresetSaveSheet::refreshSave()
{
    // A preset needs a name and something in it.
    m_save->setEnabled(!name().isEmpty() && !units().isEmpty());
}

} // namespace pixelfx::ui
