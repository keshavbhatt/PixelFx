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

#include <algorithm>

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
    // As tall as the page, scrolling only when the window is too short.
    auto* scroll = new SheetScroll(buildGeneral(), kSheetPadding, this);
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

void SettingsSheet::addPerformance(QFormLayout* form, QWidget* page)
{
    form->addRow(sectionHeading(tr("Performance"), page));
    auto* atOnce = new QComboBox(page);
    atOnce->setAccessibleName(tr("Photos at a time"));
    atOnce->addItem(tr("Automatic: 2, or 4 for Compress only"), 0);
    for (const int n : {1, 2, 4, 8}) {
        atOnce->addItem(QString::number(n), n);
    }
    atOnce->setCurrentIndex(std::max(0, atOnce->findData(m_settings.batchAtOnce())));
    connect(atOnce, &QComboBox::activated, this,
            [this, atOnce] { m_settings.setBatchAtOnce(atOnce->currentData().toInt()); });
    auto* atOnceNote = new QLabel(tr("More at once is faster with many small photos and needs more memory: "
                                     "a 24 megapixel photo takes about 1 GB while it is edited."),
                                  page);
    atOnceNote->setObjectName(u"itemDetail"_s);
    atOnceNote->setWordWrap(true);
    auto* atOnceBox = new QWidget(page);
    auto* atOnceColumn = new QVBoxLayout(atOnceBox);
    atOnceColumn->setContentsMargins(0, 0, 0, 0);
    atOnceColumn->setSpacing(kGapSmall);
    atOnceColumn->addWidget(atOnce);
    atOnceColumn->addWidget(atOnceNote);
    form->addRow(tr("Photos at a time"), atOnceBox);
}

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
    form->addRow(tr("Theme"),
                 segments({tr("System"), tr("Dark"), tr("Light")}, static_cast<int>(m_settings.theme()), page,
                          [this](int i) { m_settings.setTheme(static_cast<S::Theme>(i)); }));
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

    addPerformance(form, page);
    setLabelColumn(form, kLabelColumn);
    return page;
}

} // namespace pixelfx::ui
