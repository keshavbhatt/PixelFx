#include "ui/settings_sheet.h"

#include "core/settings/settings.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text.toUpper(), parent);
    label->setObjectName(u"section"_s);
    label->setContentsMargins(0, 14, 0, 4);
    return label;
}

/// A row of segment buttons, one checked; `chosen(index)` on click.
template <typename Chosen>
QWidget* segments(const QStringList& names, int current, QWidget* parent, Chosen chosen)
{
    auto* box = new QWidget(parent);
    auto* row = new QHBoxLayout(box);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    auto* group = new QButtonGroup(box);
    for (int i = 0; i < names.size(); ++i) {
        auto* b = new QPushButton(names.at(i), box);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        b->setChecked(i == current);
        group->addButton(b, i);
        row->addWidget(b);
    }
    row->addStretch();
    QObject::connect(group, &QButtonGroup::idClicked, box, chosen);
    return box;
}

} // namespace

SettingsSheet::SettingsSheet(core::Settings& settings, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Settings"));
    resize(760, 560);
    auto* title = new QLabel(tr("Settings"), this);
    title->setObjectName(u"sheetTitle"_s);
    auto* close = new QPushButton(tr("Close"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    auto* head = new QHBoxLayout;
    head->setContentsMargins(20, 14, 14, 10);
    head->addWidget(title);
    head->addStretch();
    head->addWidget(close);

    auto* pages = new QListWidget(this);
    pages->setObjectName(u"settingsPages"_s);
    pages->setFixedWidth(200);
    pages->addItem(tr("General"));
    pages->setCurrentRow(0);
    pages->setAccessibleName(tr("Settings pages"));
    auto* scroll = new QScrollArea(this);
    scroll->setWidget(buildGeneral());
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(pages);
    body->addWidget(scroll, 1);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(head);
    column->addLayout(body, 1);
}

SettingsSheet::~SettingsSheet() = default;

QWidget* SettingsSheet::buildGeneral()
{
    using S = core::Settings;
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);
    form->setContentsMargins(22, 4, 22, 16);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(10);

    form->addRow(sectionLabel(tr("Appearance"), page));
    auto* theme = segments({tr("System"), tr("Dark"), tr("Light")}, static_cast<int>(m_settings.theme()),
                           page, [this](int i) { m_settings.setTheme(static_cast<S::Theme>(i)); });
    auto* themeNote = new QLabel(tr("Takes effect the next time PixelFx starts."), page);
    themeNote->setObjectName(u"faint"_s);
    auto* themeBox = new QVBoxLayout;
    themeBox->setSpacing(2);
    themeBox->addWidget(theme);
    themeBox->addWidget(themeNote);
    form->addRow(tr("Theme"), themeBox);
    form->addRow(tr("Canvas background"),
                 segments({tr("Dark gray"), tr("Middle gray"), tr("Black"), tr("White")},
                          static_cast<int>(m_settings.canvasBackground()), page, [this](int i) {
                              m_settings.setCanvasBackground(static_cast<S::CanvasBackground>(i));
                          }));

    form->addRow(sectionLabel(tr("Editing"), page));
    auto* remember = new QCheckBox(tr("Keep each photo's settings in the app's library"), page);
    remember->setChecked(m_settings.rememberEdits());
    connect(remember, &QCheckBox::toggled, this, [this](bool on) { m_settings.setRememberEdits(on); });
    form->addRow(tr("Remember edits"), remember);
    auto* onOpen = new QComboBox(page);
    onOpen->addItems({tr("Show the photo as it was left"), tr("Always start from the original")});
    onOpen->setCurrentIndex(m_settings.openAsLeft() ? 0 : 1);
    onOpen->setEnabled(m_settings.rememberEdits());
    connect(remember, &QCheckBox::toggled, onOpen, &QWidget::setEnabled);
    connect(onOpen, &QComboBox::activated, this, [this](int i) { m_settings.setOpenAsLeft(i == 0); });
    form->addRow(tr("On open"), onOpen);

    form->addRow(sectionLabel(tr("Display"), page));
    auto* clipping = new QCheckBox(tr("Show clipped highlights and shadows (J)"), page);
    clipping->setChecked(m_settings.clippingWarnings());
    connect(clipping, &QCheckBox::toggled, this, [this](bool on) { m_settings.setClippingWarnings(on); });
    connect(&m_settings, &S::clippingWarningsChanged, clipping, &QCheckBox::setChecked);
    form->addRow(tr("Clipping warnings"), clipping);
    return page;
}

} // namespace pixelfx::ui
