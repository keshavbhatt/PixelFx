#include "ui/settings_sheet.h"

#include "core/settings/settings.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kLabelColumn = 160;

/// A segmented control across the column, one segment checked; `chosen(index)` on click.
template <typename Chosen>
QWidget* segments(const QStringList& names, int current, QWidget* parent, Chosen chosen)
{
    auto* group = new QButtonGroup(parent);
    QFrame* box = segmentBox(names, current, group, parent, SegmentFit::Row);
    QObject::connect(group, &QButtonGroup::idClicked, box, chosen);
    return box;
}

} // namespace

SettingsSheet::SettingsSheet(core::Settings& settings, QWidget* parent)
    : Sheet(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Settings"));
    QWidget* head = sheetHeaderBar(this, sheetTitleRow(this, tr("Settings")));

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
    column->addWidget(head);
    column->addLayout(body, 1);
    setSize(Size::Wide);
}

SettingsSheet::~SettingsSheet() = default;

QWidget* SettingsSheet::buildGeneral()
{
    using S = core::Settings;
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);
    form->setContentsMargins(kSheetPadding, 4, kSheetPadding, 16);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setHorizontalSpacing(kGapItems);
    form->setVerticalSpacing(kGapGroups);

    form->addRow(sectionHeading(tr("Appearance"), page));
    // The theme applies at the next start: said once it is changed.
    auto* themeNote = new QLabel(tr("Takes effect the next time PixelFx starts."), page);
    themeNote->setObjectName(u"faint"_s);
    themeNote->hide();
    auto* theme = segments({tr("System"), tr("Dark"), tr("Light")}, static_cast<int>(m_settings.theme()),
                           page, [this, themeNote](int i) {
                               m_settings.setTheme(static_cast<S::Theme>(i));
                               themeNote->show();
                           });
    theme->setToolTip(themeNote->text());
    auto* themeBox = new QVBoxLayout;
    themeBox->setSpacing(kGapTight);
    themeBox->addWidget(theme);
    themeBox->addWidget(themeNote);
    form->addRow(tr("Theme"), themeBox);
    form->addRow(tr("Canvas background"),
                 segments({tr("Dark gray"), tr("Middle gray"), tr("Black"), tr("White")},
                          static_cast<int>(m_settings.canvasBackground()), page, [this](int i) {
                              m_settings.setCanvasBackground(static_cast<S::CanvasBackground>(i));
                          }));

    form->addRow(sectionHeading(tr("Editing"), page));
    auto* remember = new QCheckBox(tr("Keep each photo's settings in the app's library"), page);
    remember->setObjectName(u"switch"_s);
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

    form->addRow(sectionHeading(tr("Display"), page));
    auto* clipping = new QCheckBox(tr("Show clipped highlights and shadows (J)"), page);
    clipping->setObjectName(u"switch"_s);
    clipping->setChecked(m_settings.clippingWarnings());
    connect(clipping, &QCheckBox::toggled, this, [this](bool on) { m_settings.setClippingWarnings(on); });
    connect(&m_settings, &S::clippingWarningsChanged, clipping, &QCheckBox::setChecked);
    form->addRow(tr("Clipping warnings"), clipping);
    setLabelColumn(form, kLabelColumn);
    return page;
}

} // namespace pixelfx::ui
