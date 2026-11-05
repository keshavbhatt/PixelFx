#include "ui/shortcuts_sheet.h"

#include "ui/icons.h"
#include "ui/style.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <array>
#include <vector>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

struct Shortcut
{
    const char* name;
    const char* keys;
};

struct Section
{
    const char* title;
    std::vector<Shortcut> rows;
};

// The mock's sections and wording; key names are not translated.
const std::array<Section, 4>& sections()
{
    static const std::array<Section, 4> list = {{
        {QT_TRANSLATE_NOOP("ShortcutsSheet", "Files"),
         {{QT_TRANSLATE_NOOP("ShortcutsSheet", "Open images"), "Ctrl+O"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Open folder"), "Ctrl+Shift+O"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Export"), "Ctrl+Shift+E"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Paste image"), "Ctrl+V"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Settings"), "Ctrl+,"}}},
        {QT_TRANSLATE_NOOP("ShortcutsSheet", "Editing"),
         {{QT_TRANSLATE_NOOP("ShortcutsSheet", "Undo / Redo"), "Ctrl+Z / Ctrl+Shift+Z"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "History"), "H"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Copy / Paste settings"), "Ctrl+Shift+C / V"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Reset all"), "Ctrl+R"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Auto"), "Ctrl+Shift+A"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Pick a neutral point"), "W"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Reset slider"), "Double-click"}}},
        {QT_TRANSLATE_NOOP("ShortcutsSheet", "View"),
         {{QT_TRANSLATE_NOOP("ShortcutsSheet", "Before and after"), "\\"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Show original"), "Space (hold)"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Fit / 100%"), "Ctrl+0 / Ctrl+1"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Zoom"), "Ctrl+Plus / Minus"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Full screen"), "F11"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Hide panels"), "Tab"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Main menu"), "F10"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Clipping"), "J"}}},
        {QT_TRANSLATE_NOOP("ShortcutsSheet", "Modes"),
         {{QT_TRANSLATE_NOOP("ShortcutsSheet", "Edit"), "E"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Crop"), "C"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Heal"), "R"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Heal: brush size"), "[ / ]"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Heal: show spots, reveal dust"), "V / D"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Batch"), "B"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Next / previous photo"), "Right / Left"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Select all photos"), "Ctrl+A"},
          {QT_TRANSLATE_NOOP("ShortcutsSheet", "Keyboard shortcuts"), "F1"}}},
    }};
    return list;
}

} // namespace

ShortcutsSheet::ShortcutsSheet(QWidget* parent)
    : QDialog(parent)
    , m_search(new QLineEdit(this))
{
    setWindowTitle(tr("Keyboard shortcuts"));
    auto* title = new QLabel(tr("Keyboard shortcuts"), this);
    title->setObjectName(u"sheetTitle"_s);
    m_search->setPlaceholderText(tr("Search"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(icons::themed(u"search"_s, tokens().muted), QLineEdit::LeadingPosition);
    m_search->setFixedWidth(240);
    connect(m_search, &QLineEdit::textChanged, this, &ShortcutsSheet::filter);
    auto* close = new QPushButton(tr("Close"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    auto* head = new QHBoxLayout;
    head->addWidget(title);
    head->addSpacing(12);
    head->addWidget(m_search);
    head->addStretch();
    head->addWidget(close);

    // Two columns of sections, as the mock lays them out.
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(32);
    grid->setVerticalSpacing(4);
    for (std::size_t s = 0; s < sections().size(); ++s) {
        const Section& section = sections().at(s);
        auto* column = new QVBoxLayout;
        column->setSpacing(4);
        auto* label = new QLabel(tr(section.title).toUpper(), this);
        label->setObjectName(u"section"_s);
        label->setContentsMargins(0, 10, 0, 2);
        column->addWidget(label);
        for (const Shortcut& sc : section.rows) {
            auto* row = new QWidget(this);
            row->setObjectName(u"shortcutRow"_s);
            row->setProperty("name", tr(sc.name));
            auto* name = new QLabel(tr(sc.name), row);
            auto* keys = new QLabel(QString::fromLatin1(sc.keys), row);
            keys->setObjectName(u"kbd"_s);
            auto* line = new QHBoxLayout(row);
            line->setContentsMargins(0, 2, 0, 2);
            line->addWidget(name, 1);
            line->addWidget(keys);
            column->addWidget(row);
        }
        column->addStretch();
        grid->addLayout(column, static_cast<int>(s / 2), static_cast<int>(s % 2));
    }
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 18, 22, 18);
    layout->addLayout(head);
    layout->addLayout(grid);
    setMinimumWidth(720);
}

ShortcutsSheet::~ShortcutsSheet() = default;

int ShortcutsSheet::filter(const QString& text)
{
    int shown = 0;
    for (QWidget* row : findChildren<QWidget*>(u"shortcutRow"_s)) {
        const bool match =
            text.isEmpty() || row->property("name").toString().contains(text, Qt::CaseInsensitive);
        row->setVisible(match);
        shown += match ? 1 : 0;
    }
    return shown;
}

} // namespace pixelfx::ui
