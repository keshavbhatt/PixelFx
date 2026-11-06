#include "ui/name_field.h"

#include "ui/elided_label.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kShown = 3;

// "holiday.jpg" again becomes "holiday (2).jpg", as the batch numbers a name already taken.
QString numbered(const QString& name, int n)
{
    const QFileInfo info(name);
    return u"%1 (%2).%3"_s.arg(info.completeBaseName()).arg(n).arg(info.suffix());
}

} // namespace

NameField::NameField(QWidget* parent)
    : QWidget(parent)
    , m_field(new QLineEdit(this))
    , m_token(new QPushButton(icons::themed(u"plus"_s, tokens().text), tr("Token"), this))
    , m_rows(new QGridLayout)
    , m_more(new QLabel(this))
    , m_clash(new QWidget(this))
    , m_overwrites(new QWidget(this))
    , m_tokens(core::nameTokens())
{
    m_field->setObjectName(u"monoField"_s);
    m_field->setAccessibleName(tr("File name"));
    m_token->setObjectName(u"small"_s);
    m_token->setToolTip(tr("Insert a token at the cursor"));
    m_token->setAccessibleName(tr("Insert a token"));
    connect(m_token, &QPushButton::clicked, this, &NameField::showTokenMenu);
    connect(m_field, &QLineEdit::textChanged, this, &NameField::refresh);
    connect(m_field, &QLineEdit::editingFinished, this, [this] { Q_EMIT edited(nameTemplate()); });

    auto* fieldRow = new QHBoxLayout;
    fieldRow->setSpacing(kGapRelated);
    fieldRow->addWidget(m_field, 1);
    fieldRow->addWidget(m_token);

    m_rows->setContentsMargins(0, 2, 0, 0);
    m_rows->setHorizontalSpacing(6);
    m_rows->setVerticalSpacing(4);
    // The new name at the right, whole when it fits; when a name is long the source gives way
    // first, both cut in the middle so their ends stay.
    m_rows->setColumnStretch(0, 1);
    for (int i = 0; i < kShown; ++i) {
        auto* source = new ElidedLabel(this);
        source->setObjectName(u"monoMuted"_s);
        source->setElideMode(Qt::ElideMiddle);
        auto* arrow = new QLabel(u"→"_s, this);
        arrow->setObjectName(u"faint"_s);
        auto* name = new ElidedLabel(this);
        name->setObjectName(u"mono"_s);
        name->setElideMode(Qt::ElideMiddle);
        name->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred); // its width, if there is room
        m_rows->addWidget(source, i, 0);
        m_rows->addWidget(arrow, i, 1);
        m_rows->addWidget(name, i, 2);
    }
    m_more->setObjectName(u"faintSmall"_s);

    warningLine(m_clash, tr("Every photo gets this name, so numbers are added. Add {n} to choose where."));
    m_overwritesText = warningLine(m_overwrites, QString());
    m_overwrites->hide();

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(kGapSmall);
    column->addLayout(fieldRow);
    column->addLayout(m_rows);
    column->addWidget(m_more);
    column->addWidget(m_clash);
    column->addWidget(m_overwrites);
    refresh();
}

QLabel* NameField::warningLine(QWidget* line, const QString& text)
{
    auto* icon = new QLabel(line);
    icons::setGlyph(icon, u"alert"_s, tokens().warning, 14);
    auto* label = new QLabel(text, line);
    label->setObjectName(u"warningText"_s);
    label->setWordWrap(true);
    auto* row = new QHBoxLayout(line);
    row->setContentsMargins(0, 2, 0, 0);
    row->setSpacing(6);
    row->addWidget(icon, 0, Qt::AlignTop);
    row->addWidget(label, 1);
    return label;
}

void NameField::setOverwrites(int count)
{
    //: Under the name preview when If it exists is Overwrite and names are the originals' own.
    m_overwritesText->setText(
        tr("%n photos would be saved over their originals: Overwrite replaces them.", nullptr, count));
    m_overwrites->setVisible(count > 0);
}

bool NameField::showsOverwrites() const
{
    return !m_overwrites->isHidden();
}

NameField::~NameField() = default;

void NameField::setTemplate(const QString& nameTemplate)
{
    if (nameTemplate != m_field->text()) {
        m_field->setText(nameTemplate); // refreshes
    }
}

QString NameField::nameTemplate() const
{
    return m_field->text().trimmed();
}

void NameField::setTokens(const QStringList& tokens)
{
    m_tokens = tokens;
}

void NameField::setFiles(const QStringList& files, Namer namer)
{
    m_files = files;
    m_namer = std::move(namer);
    refresh();
}

core::NameFacts NameField::factsOf(int index)
{
    const QString& path = m_files.at(index);
    auto it = m_facts.find(path);
    if (it == m_facts.end()) {
        const auto metadata = pfx::readMetadata(path.toStdString());
        it = m_facts.insert(path, metadata ? core::nameFacts(metadata.value()) : core::NameFacts{});
    }
    core::NameFacts facts = *it;
    facts.number = index + 1;
    facts.count = static_cast<int>(m_files.size());
    return facts;
}

void NameField::refresh()
{
    const int count = static_cast<int>(m_files.size());
    const int shown = m_namer ? std::min(kShown, count) : 0;
    QSet<QString> taken;
    for (int i = 0; i < kShown; ++i) {
        auto* source = static_cast<QLabel*>(m_rows->itemAtPosition(i, 0)->widget());
        auto* arrow = m_rows->itemAtPosition(i, 1)->widget();
        auto* name = static_cast<QLabel*>(m_rows->itemAtPosition(i, 2)->widget());
        const bool on = i < shown;
        for (QWidget* w : {static_cast<QWidget*>(source), arrow, static_cast<QWidget*>(name)}) {
            w->setVisible(on);
        }
        if (!on) {
            continue;
        }
        const QString file = QFileInfo(m_files.at(i)).fileName();
        QString made = m_namer(nameTemplate(), m_files.at(i), factsOf(i));
        for (int n = 2; taken.contains(made.toLower()); ++n) {
            made = numbered(m_namer(nameTemplate(), m_files.at(i), factsOf(i)), n);
        }
        taken.insert(made.toLower());
        source->setText(file);
        name->setText(made);
        source->setToolTip(file); // whole, when cut short
        name->setToolTip(made);
        //: A name preview row read by screen readers: "IMG_0412.HEIC becomes trip-001.jpg".
        source->setAccessibleName(tr("%1 becomes %2").arg(file, made));
    }
    m_more->setText(tr("and %n more", nullptr, count - shown));
    m_more->setVisible(count > shown && shown > 0);
    m_clash->setVisible(count > 1 && !core::namesDiffer(nameTemplate()));
}

QStringList NameField::shownNames() const
{
    QStringList out;
    for (int i = 0; i < kShown; ++i) {
        const auto* source = static_cast<QLabel*>(m_rows->itemAtPosition(i, 0)->widget());
        const auto* name = static_cast<QLabel*>(m_rows->itemAtPosition(i, 2)->widget());
        if (!source->isHidden()) {
            out << source->text() + u" → "_s + name->text();
        }
    }
    return out;
}

bool NameField::showsClash() const
{
    return !m_clash->isHidden();
}

QString NameField::meaningOf(const QString& token)
{
    if (token == u"{name}"_s) {
        return tr("The file's own name");
    }
    if (token == u"{n}"_s) {
        return tr("Its number in the batch");
    }
    if (token == u"{taken}"_s) {
        return tr("The day it was taken");
    }
    if (token == u"{date}"_s) {
        return tr("Today");
    }
    if (token == u"{camera}"_s) {
        return tr("The camera");
    }
    if (token == u"{w}x{h}"_s) {
        return tr("The saved size");
    }
    return tr("The recipe's name");
}

void NameField::showTokenMenu()
{
    QMenu menu(this);
    for (const QString& token : m_tokens) {
        QString example;
        if (m_namer && !m_files.isEmpty()) {
            example = QFileInfo(m_namer(token, m_files.first(), factsOf(0))).completeBaseName();
        }
        // The example after a tab shows in the menu's right-hand column.
        QAction* a = menu.addAction(u"%1   %2\t%3"_s.arg(token, meaningOf(token), example));
        a->setData(token);
    }
    if (const QAction* chosen = menu.exec(m_token->mapToGlobal(QPoint(0, m_token->height())))) {
        m_field->insert(chosen->data().toString());
        m_field->setFocus();
        Q_EMIT edited(nameTemplate());
    }
}

} // namespace pixelfx::ui
