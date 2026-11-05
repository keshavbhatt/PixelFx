#include "ui/heal_panel.h"

#include "ui/heal_tool.h"
#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/list_row.h"
#include "ui/slider_row.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>

#include <cmath>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kPanelWidth = 316;
constexpr int kSide = 14;
constexpr int kSizeTicks = 1000; // the size slider counts tenths of a percent

// A size as a share of the long edge, as people read it: "4%", "2.5%".
QString percent(double share)
{
    const double p = share * 100.0;
    return std::abs(p - std::round(p)) < 0.05 ? HealPanel::tr("%1%").arg(qRound(p))
                                              : HealPanel::tr("%1%").arg(p, 0, 'f', 1);
}

QLabel* heading(const QString& text, QWidget* parent)
{
    QLabel* label = sectionLabel(text, parent);
    label->setContentsMargins(kSide, 12, kSide, 4);
    return label;
}

} // namespace

HealPanel::HealPanel(HealTool* tool, QWidget* parent)
    : QWidget(parent)
    , m_tool(tool)
{
    setObjectName(u"healPanel"_s);
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(kPanelWidth);
    auto* reset = new QPushButton(tr("Reset"), this);
    reset->setToolTip(tr("Take off the spots made since Heal opened"));
    auto* done = new QPushButton(tr("Done"), this);
    done->setObjectName(u"primary"_s);
    done->setToolTip(tr("Keep the spots and leave Heal (Escape)"));
    connect(reset, &QPushButton::clicked, this, &HealPanel::resetRequested);
    connect(done, &QPushButton::clicked, this, &HealPanel::doneRequested);
    auto* foot = new QHBoxLayout;
    foot->addWidget(reset);
    foot->addStretch();
    foot->addWidget(done);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(heading(tr("Heal"), this));
    column->addWidget(buildBrush());
    column->addWidget(buildSpots(), 1);
    column->addWidget(sheetFootBar(this, foot));
    connect(m_tool, &HealTool::spotsChanged, this, [this](const pfx::Spots& /*spots*/, bool finished) {
        if (finished) {
            refresh();
        }
    });
    connect(m_tool, &HealTool::selectionChanged, this, [this] { refresh(); });
    connect(m_tool, &HealTool::brushChanged, this, &HealPanel::refreshValues);
    refresh();
}

HealPanel::~HealPanel() = default;

QWidget* HealPanel::buildBrush()
{
    auto* box = new QWidget(this);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(kSide, 0, kSide, 4);
    column->setSpacing(8);
    auto* modes = new QHBoxLayout;
    modes->setSpacing(0);
    m_modes = new QButtonGroup(this);
    const QStringList names = {tr("Spot"), tr("Brush")};
    const QStringList glyphs = {u"circle-dot"_s, u"brush"_s};
    const QStringList tips = {tr("Click a blemish to heal it; a patch is picked for you"),
                              tr("Paint along a longer mark (a wire, a scratch, a hair) to heal it")};
    for (int i = 0; i < 2; ++i) {
        auto* b =
            new IconTextButton(icons::toggle(glyphs.at(i), tokens().text, tokens().accent), names.at(i), box);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        b->setChecked(i == 0);
        b->setToolTip(tips.at(i));
        m_modes->addButton(b, i);
        modes->addWidget(b, 1);
    }
    connect(m_modes, &QButtonGroup::idClicked, this,
            [this](int id) { m_tool->setMode(id == 0 ? HealTool::Mode::Spot : HealTool::Mode::Brush); });
    column->addLayout(modes);
    m_size = new SliderRow(tr("Size"), box);
    m_size->setRange(qRound(HealTool::kMinSize * kSizeTicks), qRound(HealTool::kMaxSize * kSizeTicks), 40);
    m_size->setFormat([](int ticks) { return percent(static_cast<double>(ticks) / kSizeTicks); });
    m_size->setTicksPerUnit(kSizeTicks / 100.0); // typed in percent
    m_size->setToolTip(tr("[ and ] change the size"));
    m_feather = new SliderRow(tr("Feather"), box);
    m_feather->setRange(0, 100, 50);
    m_opacity = new SliderRow(tr("Opacity"), box);
    m_opacity->setRange(1, 100, 100);
    // Live while dragging, one history step when let go, as the adjustment sliders.
    const auto wire = [this](SliderRow* row, void (HealTool::*set)(double, bool), double scale) {
        connect(row, &SliderRow::ticksMoved, this, [this, row, set, scale](int t) {
            if (!m_refreshing && row->slider()->isSliderDown()) {
                (m_tool->*set)(t / scale, false);
            }
        });
        connect(row, &SliderRow::ticksReleased, this, [this, set, scale](int t) {
            if (!m_refreshing) {
                (m_tool->*set)(t / scale, true);
            }
        });
    };
    wire(m_size, &HealTool::setSize, kSizeTicks);
    wire(m_feather, &HealTool::setFeather, 1.0);
    wire(m_opacity, &HealTool::setOpacity, 1.0);
    for (SliderRow* row : {m_size, m_feather, m_opacity}) {
        column->addWidget(row);
    }
    column->addWidget(buildSwitch(tr("Show spots"), u"V"_s, &m_show));
    column->addWidget(buildSwitch(tr("Reveal dust"), u"D"_s, &m_reveal));
    m_show->setChecked(true);
    m_reveal->setToolTip(tr("A view where dust and specks stand out, for skies and plain walls. "
                            "Only the view changes, not the photo."));
    connect(m_show, &QCheckBox::toggled, this, &HealPanel::showSpotsToggled);
    connect(m_reveal, &QCheckBox::toggled, this, &HealPanel::revealDustToggled);
    return box;
}

QWidget* HealPanel::buildSwitch(const QString& label, const QString& key, QCheckBox** box)
{
    auto* row = new QWidget(this);
    *box = new QCheckBox(label, row);
    (*box)->setObjectName(u"switch"_s);
    auto* keycap = new QLabel(key, row);
    keycap->setObjectName(u"kbd"_s);
    auto* line = new QHBoxLayout(row);
    line->setContentsMargins(0, 0, 0, 0);
    line->addWidget(*box);
    line->addStretch();
    line->addWidget(keycap);
    return row;
}

QWidget* HealPanel::buildSpots()
{
    auto* box = new QWidget(this);
    m_count = sectionLabel(QString(), box);
    m_removeAll = new QPushButton(tr("Remove all"), box);
    m_removeAll->setObjectName(u"link"_s);
    connect(m_removeAll, &QPushButton::clicked, m_tool, &HealTool::removeAll);
    auto* head = new QHBoxLayout;
    head->setContentsMargins(kSide, 12, kSide - 4, 4);
    head->addWidget(m_count, 1);
    head->addWidget(m_removeAll);
    auto* listBox = new QWidget(box);
    listBox->setObjectName(u"adjustContent"_s);
    m_list = new QVBoxLayout(listBox);
    m_list->setContentsMargins(8, 0, 8, 8);
    m_list->setSpacing(2);
    m_list->addStretch();
    auto* scroll = new QScrollArea(box);
    scroll->setObjectName(u"adjustScroll"_s);
    scroll->setWidget(listBox);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(head);
    column->addWidget(scroll, 1);
    return box;
}

void HealPanel::refresh()
{
    m_refreshing = true;
    const pfx::Spots& spots = m_tool->spots();
    m_count->setText(tr("Spots (%1)").arg(spots.size()).toUpper());
    m_removeAll->setVisible(!spots.empty());
    // A row may be the one being clicked: it goes once the event is over.
    for (ListRow* row : m_rows) {
        row->hide();
        row->deleteLater();
    }
    m_rows.clear();
    for (std::size_t i = 0; i < spots.size(); ++i) {
        const pfx::Spot& s = spots[i];
        const bool stroke = s.stroke.size() >= 2;
        auto* row = new ListRow(m_list->parentWidget());
        row->setIcon(stroke ? u"brush"_s : u"circle-dot"_s);
        row->setTitle(stroke ? tr("Brush stroke %1").arg(i + 1) : tr("Spot %1").arg(i + 1));
        row->setSubtitle(tr("Size %1").arg(percent(s.radius * 2.0)));
        row->setAction(u"close"_s, tr("Remove this spot"));
        const int index = static_cast<int>(i);
        row->setSelected(index == m_tool->selected());
        connect(row, &ListRow::clicked, this, [this, index] { m_tool->select(index); });
        connect(row, &ListRow::actionClicked, this, [this, index] {
            m_tool->select(index);
            m_tool->removeSelected();
        });
        m_list->insertWidget(m_list->count() - 1, row); // before the stretch
        m_rows.push_back(row);
    }
    m_refreshing = false;
    refreshValues();
}

void HealPanel::refreshValues()
{
    m_refreshing = true;
    m_size->setTicks(qRound(m_tool->size() * kSizeTicks));
    m_feather->setTicks(qRound(m_tool->feather()));
    m_opacity->setTicks(qRound(m_tool->opacity()));
    m_refreshing = false;
}

void HealPanel::setShowSpots(bool on)
{
    m_show->setChecked(on);
}

void HealPanel::setRevealDust(bool on)
{
    m_reveal->setChecked(on);
}

bool HealPanel::revealDust() const
{
    return m_reveal->isChecked();
}

} // namespace pixelfx::ui
