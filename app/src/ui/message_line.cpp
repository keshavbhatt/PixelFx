#include "ui/message_line.h"

#include "ui/elided_label.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

MessageLine::MessageLine(QWidget* parent)
    : QWidget(parent)
    , m_icon(new QLabel(this))
    , m_text(new ElidedLabel(this))
{
    m_text->setObjectName(u"messageText"_s);
    m_text->setAccessibleName(tr("Message"));
    m_icon->hide();
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(kGapRelated);
    row->addWidget(m_icon);
    row->addWidget(m_text, 1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    show(Kind::Note, QString());
}

MessageLine::~MessageLine() = default;

void MessageLine::setNote(const QString& note)
{
    m_note = note;
    if (m_kind == Kind::Note) {
        show(Kind::Note, note);
    }
}

void MessageLine::showInfo(const QString& text)
{
    show(Kind::Info, text);
}

void MessageLine::showError(const QString& text)
{
    show(Kind::Error, text);
}

void MessageLine::clear()
{
    show(Kind::Note, m_note);
}

QString MessageLine::text() const
{
    return m_text->text();
}

void MessageLine::show(Kind kind, const QString& text)
{
    m_kind = kind;
    m_text->setText(text);
    m_text->setToolTip(text);
    const char* names[] = {"note", "info", "error"};
    m_text->setProperty("kind", QString::fromLatin1(names[static_cast<int>(kind)]));
    m_text->style()->unpolish(m_text);
    m_text->style()->polish(m_text);
    if (kind == Kind::Note) {
        m_icon->hide();
    } else {
        const QColor colour = kind == Kind::Error ? tokens().danger : tokens().muted;
        icons::setGlyph(m_icon, kind == Kind::Error ? u"alert"_s : u"info"_s, colour, 14);
        m_icon->show();
    }
}

} // namespace pixelfx::ui
