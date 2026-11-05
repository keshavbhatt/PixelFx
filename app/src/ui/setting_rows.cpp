#include "ui/setting_rows.h"

#include "ui/style.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

ChoiceRow::ChoiceRow(const QString& name, std::vector<std::string> values, const QStringList& texts,
                     QWidget* parent)
    : QWidget(parent)
    , m_values(std::move(values))
    , m_list(new QComboBox(this))
{
    Q_ASSERT(!m_values.empty() && static_cast<std::size_t>(texts.size()) == m_values.size());
    auto* label = new QLabel(name, this);
    label->setObjectName(u"adjustName"_s);
    label->setBuddy(m_list);
    m_list->setObjectName(u"compact"_s);
    m_list->addItems(texts);
    m_list->setAccessibleName(name);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 5, 0, 5);
    row->setSpacing(kGapRelated);
    row->addWidget(label);
    row->addStretch();
    row->addWidget(m_list);
    connect(m_list, &QComboBox::activated, this,
            [this](int index) { Q_EMIT chosen(m_values.at(static_cast<std::size_t>(index))); });
}

ChoiceRow::~ChoiceRow() = default;

const std::string& ChoiceRow::value() const
{
    return m_values.at(static_cast<std::size_t>(std::max(0, m_list->currentIndex())));
}

void ChoiceRow::setValue(const std::string& value)
{
    const auto it = std::ranges::find(m_values, value);
    const QSignalBlocker block(m_list);
    m_list->setCurrentIndex(it == m_values.end() ? 0 : static_cast<int>(it - m_values.begin()));
}

CheckRow::CheckRow(const QString& name, const QString& description, QWidget* parent)
    : QWidget(parent)
    , m_box(new QCheckBox(name, this))
{
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 6, 0, 6);
    column->setSpacing(kGapTight);
    column->addWidget(m_box);
    if (!description.isEmpty()) {
        auto* note = new QLabel(description, this);
        note->setObjectName(u"checkNote"_s);
        note->setWordWrap(true);
        m_box->setToolTip(description);
        column->addWidget(note);
    }
    connect(m_box, &QCheckBox::toggled, this, &CheckRow::toggled);
}

CheckRow::~CheckRow() = default;

bool CheckRow::isChecked() const
{
    return m_box->isChecked();
}

void CheckRow::setChecked(bool on)
{
    const QSignalBlocker block(m_box);
    m_box->setChecked(on);
}

} // namespace pixelfx::ui
