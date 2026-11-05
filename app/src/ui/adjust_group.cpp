#include "ui/adjust_group.h"

#include "ui/adjust_panel.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

// The header row: a click anywhere on it, not only on the name, opens or closes the group.
class HeaderRow : public QWidget
{
public:
    HeaderRow(QWidget* parent, std::function<void()> onClick)
        : QWidget(parent)
        , m_onClick(std::move(onClick))
    {
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            m_onClick();
        }
    }

private:
    std::function<void()> m_onClick;
};

} // namespace

AdjustGroup::AdjustGroup(const QString& name, const QString& glyph, QWidget* parent)
    : QWidget(parent)
    , m_name(name)
    , m_toggle(new QToolButton(this))
    , m_chevron(new QLabel(this))
    , m_dot(new QLabel(this))
    , m_reset(new QToolButton(this))
    , m_body(new QWidget(this))
    , m_bodyLayout(new QVBoxLayout(m_body))
{
    setObjectName(u"adjustGroup"_s);
    setAttribute(Qt::WA_StyledBackground);
    auto* header = new HeaderRow(this, [this] { m_toggle->toggle(); });
    m_toggle->setParent(header);
    m_chevron->setParent(header);
    m_dot->setParent(header);
    m_reset->setParent(header);
    m_toggle->setObjectName(u"groupToggle"_s);
    m_toggle->setText(name);
    m_toggle->setCheckable(true);
    m_toggle->setChecked(true);
    m_toggle->setAccessibleName(name);
    m_toggle->setToolTip(tr("Show or hide %1").arg(name));
    auto* icon = new QLabel(header);
    icon->setPixmap(icons::pixmap(glyph, tokens().muted, 15, devicePixelRatioF()));
    m_dot->setObjectName(u"changedDot"_s);
    m_dot->setFixedSize(6, 6);
    m_dot->setToolTip(tr("Changed"));
    m_reset->setObjectName(u"groupReset"_s);
    m_reset->setIcon(icons::themed(u"reset"_s, tokens().muted));
    m_reset->setToolTip(tr("Reset %1").arg(name));
    m_reset->setAccessibleName(tr("Reset %1").arg(name));
    auto* row = new QHBoxLayout(header);
    row->setContentsMargins(14, 10, 14, 10);
    row->setSpacing(8);
    row->addWidget(m_chevron);
    row->addWidget(icon);
    row->addWidget(m_toggle);
    row->addWidget(m_dot);
    row->addStretch();
    row->addWidget(m_reset);
    m_bodyLayout->setContentsMargins(14, 0, 14, 10);
    m_bodyLayout->setSpacing(0);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(header);
    column->addWidget(m_body);
    connect(m_toggle, &QToolButton::toggled, this, [this](bool open) {
        m_body->setVisible(open);
        updateChevron();
    });
    updateChevron();
    setChanged(false);
}

AdjustGroup::~AdjustGroup() = default;

void AdjustGroup::addContent(QWidget* widget)
{
    widget->setParent(m_body);
    m_bodyLayout->addWidget(widget);
}

void AdjustGroup::addSlider(AdjustSlider* slider)
{
    addContent(slider);
    m_sliders.push_back(slider);
}

void AdjustGroup::addKeys(const std::vector<std::string>& keys)
{
    m_keys.insert(m_keys.end(), keys.begin(), keys.end());
}

void AdjustGroup::setExpanded(bool open)
{
    m_toggle->setChecked(open);
}

bool AdjustGroup::isExpanded() const
{
    return m_toggle->isChecked();
}

bool AdjustGroup::isChanged(const pfx::Settings& settings) const
{
    return std::ranges::any_of(m_sliders, [](const AdjustSlider* s) { return s->isChanged(); }) ||
           std::ranges::any_of(m_keys, [&settings](const std::string& k) { return settings.contains(k); });
}

void AdjustGroup::reset(pfx::Settings& settings) const
{
    for (AdjustSlider* s : m_sliders) {
        settings.erase(s->key().toStdString());
        s->setValue(s->defaultValue());
    }
    for (const std::string& key : m_keys) {
        settings.erase(key);
    }
}

void AdjustGroup::setChanged(bool changed)
{
    m_dot->setVisible(changed);
    m_reset->setVisible(changed);
}

void AdjustGroup::updateChevron()
{
    m_chevron->setPixmap(icons::pixmap(m_toggle->isChecked() ? u"expanded"_s : u"collapsed"_s, tokens().muted,
                                       14, devicePixelRatioF()));
}

} // namespace pixelfx::ui
