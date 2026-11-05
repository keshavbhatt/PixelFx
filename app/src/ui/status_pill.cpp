#include "ui/status_pill.h"

#include "ui/icons.h"
#include "ui/style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

StatusPill::StatusPill(QWidget* parent)
    : QFrame(parent)
    , m_glyph(new QLabel(this))
    , m_text(new QLabel(this))
{
    setObjectName(u"pill"_s);
    setFixedHeight(20);
    m_glyph->setObjectName(u"pillIcon"_s);
    m_text->setObjectName(u"pillText"_s);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(8, 2, 8, 2);
    row->setSpacing(kGapSmall);
    row->addWidget(m_glyph);
    row->addWidget(m_text);
    hide();
}

StatusPill::~StatusPill() = default;

void StatusPill::setStatus(Kind kind, const QString& text, const QString& glyph)
{
    const Tokens& t = tokens();
    QColor colour = t.muted;
    QString name = u"plain"_s;
    switch (kind) {
    case Kind::Ok:
        colour = t.success;
        name = u"ok"_s;
        break;
    case Kind::Bad:
        colour = t.danger;
        name = u"bad"_s;
        break;
    case Kind::Accent:
        colour = t.accent;
        name = u"accent"_s;
        break;
    case Kind::Plain:
        break;
    }
    m_glyph->setVisible(!glyph.isEmpty());
    if (!glyph.isEmpty()) {
        m_glyph->setPixmap(icons::pixmap(glyph, colour, 12, devicePixelRatioF()));
    }
    m_text->setText(text);
    setProperty("kind", name);
    for (QWidget* w : {static_cast<QWidget*>(this), static_cast<QWidget*>(m_text)}) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
    setVisible(!text.isEmpty());
}

QString StatusPill::text() const
{
    return m_text->text();
}

} // namespace pixelfx::ui
