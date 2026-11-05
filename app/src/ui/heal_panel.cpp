#include "ui/heal_panel.h"

#include "ui/heal_tool.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <cmath>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kPanelWidth = 316; // as the adjust and crop panels
constexpr int kSide = 14;

QLabel* section(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text.toUpper(), parent);
    label->setObjectName(u"section"_s);
    QFont f = label->font();
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.66);
    label->setFont(f);
    label->setContentsMargins(kSide, 12, kSide, 4);
    return label;
}

// Size as people read it: "4%", "2.5%".
QString percent(double share)
{
    const double p = share * 100.0;
    return std::abs(p - std::round(p)) < 0.05 ? HealPanel::tr("%1%").arg(qRound(p))
                                              : HealPanel::tr("%1%").arg(p, 0, 'f', 1);
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
    foot->setContentsMargins(kSide, 10, kSide, 12);
    foot->addWidget(reset);
    foot->addStretch();
    foot->addWidget(done);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(section(tr("Heal"), this));
    column->addWidget(buildBrush());
    column->addWidget(buildSpots(), 1);
    column->addLayout(foot);
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

QSlider* HealPanel::addSlider(const QString& name, int minimum, int maximum, QWidget* parent, QLabel** value)
{
    auto* label = new QLabel(name, parent);
    *value = new QLabel(parent);
    (*value)->setObjectName(u"sliderValue"_s);
    auto* slider = new QSlider(Qt::Horizontal, parent);
    slider->setRange(minimum, maximum);
    slider->setAccessibleName(name);
    auto* row = new QHBoxLayout;
    row->addWidget(label);
    row->addStretch();
    row->addWidget(*value);
    auto* box = qobject_cast<QVBoxLayout*>(parent->layout());
    box->addLayout(row);
    box->addWidget(slider);
    return slider;
}

QWidget* HealPanel::buildBrush()
{
    auto* box = new QWidget(this);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(kSide, 0, kSide, 4);
    column->setSpacing(4);
    auto* modes = new QHBoxLayout;
    modes->setSpacing(0);
    m_modes = new QButtonGroup(this);
    const QStringList names = {tr("Spot"), tr("Brush")};
    const QStringList tips = {tr("Click a blemish to heal it; a patch is picked for you"),
                              tr("Paint along a longer mark (a wire, a scratch, a hair) to heal it")};
    for (int i = 0; i < 2; ++i) {
        auto* b = new QPushButton(names.at(i), box);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        b->setChecked(i == 0);
        b->setToolTip(tips.at(i));
        m_modes->addButton(b, i);
        modes->addWidget(b, 1);
    }
    column->addLayout(modes);
    column->addSpacing(6);
    connect(m_modes, &QButtonGroup::idClicked, this,
            [this](int id) { m_tool->setMode(id == 0 ? HealTool::Mode::Spot : HealTool::Mode::Brush); });
    m_size = addSlider(tr("Size"), qRound(HealTool::kMinSize * 1000), qRound(HealTool::kMaxSize * 1000), box,
                       &m_sizeValue);
    m_size->setToolTip(tr("[ and ] change the size"));
    m_feather = addSlider(tr("Feather"), 0, 100, box, &m_featherValue);
    m_opacity = addSlider(tr("Opacity"), 1, 100, box, &m_opacityValue);
    // Live while dragging, one history step on release (as the adjustment sliders).
    const auto wire = [this](QSlider* slider, const std::function<void(double, bool)>& set, double scale) {
        connect(slider, &QSlider::valueChanged, this, [this, slider, set, scale](int v) {
            if (!m_refreshing) {
                set(v / scale, !slider->isSliderDown());
            }
        });
        connect(slider, &QSlider::sliderReleased, this,
                [slider, set, scale] { set(slider->value() / scale, true); });
    };
    wire(m_size, [this](double v, bool f) { m_tool->setSize(v, f); }, 1000.0);
    wire(m_feather, [this](double v, bool f) { m_tool->setFeather(v, f); }, 1.0);
    wire(m_opacity, [this](double v, bool f) { m_tool->setOpacity(v, f); }, 1.0);
    m_show = new QCheckBox(tr("Show spots (V)"), box);
    m_show->setChecked(true);
    m_reveal = new QCheckBox(tr("Reveal dust (D)"), box);
    m_reveal->setToolTip(tr("A view where dust and specks stand out, for skies and plain walls. "
                            "Only the view changes, not the photo."));
    connect(m_show, &QCheckBox::toggled, this, &HealPanel::showSpotsToggled);
    connect(m_reveal, &QCheckBox::toggled, this, &HealPanel::revealDustToggled);
    column->addSpacing(6);
    column->addWidget(m_show);
    column->addWidget(m_reveal);
    return box;
}

QWidget* HealPanel::buildSpots()
{
    auto* box = new QWidget(this);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    m_count = section(QString(), box);
    m_removeAll = new QPushButton(tr("Remove all"), box);
    m_removeAll->setObjectName(u"small"_s);
    connect(m_removeAll, &QPushButton::clicked, m_tool, &HealTool::removeAll);
    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, kSide, 0);
    head->addWidget(m_count, 1);
    head->addWidget(m_removeAll);
    m_list = new QListWidget(box);
    m_list->setObjectName(u"spotList"_s);
    m_list->setAccessibleName(tr("Spots"));
    m_list->setToolTip(tr("Delete removes the selected spot; the arrow keys on the photo move it"));
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!m_refreshing) {
            m_tool->select(row);
        }
    });
    column->addLayout(head);
    column->addWidget(m_list, 1);
    return box;
}

void HealPanel::refresh()
{
    m_refreshing = true;
    const pfx::Spots& spots = m_tool->spots();
    m_count->setText(tr("Spots (%1)").arg(spots.size()).toUpper());
    m_removeAll->setEnabled(!spots.empty());
    m_list->clear();
    for (std::size_t i = 0; i < spots.size(); ++i) {
        const pfx::Spot& s = spots[i];
        //: A row of Heal's list of spots: "Spot 2 · size 3%".
        const QString name =
            s.stroke.size() >= 2 ? tr("Brush stroke %1").arg(i + 1) : tr("Spot %1").arg(i + 1);
        m_list->addItem(tr("%1 · size %2").arg(name, percent(s.radius * 2.0)));
    }
    m_list->setCurrentRow(m_tool->selected());
    m_refreshing = false;
    refreshValues();
}

void HealPanel::refreshValues()
{
    m_refreshing = true;
    m_size->setValue(qRound(m_tool->size() * 1000));
    m_feather->setValue(qRound(m_tool->feather()));
    m_opacity->setValue(qRound(m_tool->opacity()));
    m_sizeValue->setText(percent(m_tool->size()));
    m_featherValue->setText(QString::number(qRound(m_tool->feather())));
    m_opacityValue->setText(QString::number(qRound(m_tool->opacity())));
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
