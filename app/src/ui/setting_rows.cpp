#include "ui/setting_rows.h"

#include "ui/style.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
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
    , m_note(new QLabel(description, this))
{
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 6, 0, 6);
    column->setSpacing(kGapTight);
    column->addWidget(m_box);
    m_note->setObjectName(u"checkNote"_s);
    m_note->setWordWrap(true);
    m_note->setVisible(!description.isEmpty());
    m_box->setToolTip(description);
    column->addWidget(m_note);
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

void CheckRow::setDescription(const QString& text)
{
    m_note->setText(text);
    m_note->setVisible(!text.isEmpty());
    m_box->setAccessibleDescription(text);
}

QString CheckRow::description() const
{
    return m_note->text();
}

} // namespace pixelfx::ui

namespace pixelfx::ui {

void ChoiceRow::setAvailable(const std::string& value, bool available)
{
    const auto it = std::ranges::find(m_values, value);
    if (it == m_values.end()) {
        return;
    }
    auto* view = qobject_cast<QListView*>(m_list->view());
    if (view != nullptr) {
        view->setRowHidden(static_cast<int>(it - m_values.begin()), !available);
    }
}

} // namespace pixelfx::ui
