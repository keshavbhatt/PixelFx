#include "ui/list_row.h"

#include "ui/elided_label.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

ListRow::ListRow(QWidget* parent)
    : QWidget(parent)
    , m_row(new QHBoxLayout(this))
    , m_icon(new QLabel(this))
    , m_title(new QLabel(this))
    , m_subtitle(new ElidedLabel(this))
{
    setObjectName(u"listRow"_s);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); // as tall as its content, never more
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    m_icon->hide();
    m_title->setObjectName(u"itemTitle"_s);
    m_subtitle->setObjectName(u"itemDetail"_s);
    m_subtitle->hide();
    // The title and its line stay together in the middle of a row taller than they are.
    auto* text = new QVBoxLayout;
    text->setSpacing(0);
    text->addStretch();
    text->addWidget(m_title);
    text->addWidget(m_subtitle);
    text->addStretch();
    m_row->setContentsMargins(10, 7, 10, 7);
    m_row->setSpacing(kGapItems);
    m_row->addWidget(m_icon);
    m_row->addLayout(text, 1);
}

ListRow::~ListRow() = default;

void ListRow::setIcon(const QString& glyph)
{
    icons::setGlyph(m_icon, glyph, tokens().muted, 15);
    m_icon->show();
}

void ListRow::setTitle(const QString& title)
{
    m_title->setText(title);
    setAccessibleName(title);
}

void ListRow::setSubtitle(const QString& subtitle)
{
    m_subtitle->setText(subtitle);
    m_subtitle->setToolTip(subtitle);
    m_subtitle->setVisible(!subtitle.isEmpty());
}

void ListRow::setSubtitleError(bool error)
{
    if (m_subtitle->property("error").toBool() != error) {
        m_subtitle->setProperty("error", error);
        m_subtitle->style()->unpolish(m_subtitle);
        m_subtitle->style()->polish(m_subtitle);
    }
}

void ListRow::setAction(const QString& glyph, const QString& tip)
{
    if (m_action == nullptr) {
        // The button's own 24 px target is padding enough at the row's right end.
        m_row->setContentsMargins(10, 7, 4, 7);
        m_action = new QToolButton(this);
        m_action->setObjectName(u"flatIcon"_s);
        m_row->addWidget(m_action);
        connect(m_action, &QToolButton::clicked, this, &ListRow::actionClicked);
    }
    m_action->setIcon(icons::themed(glyph, tokens().muted));
    m_action->setToolTip(tip);
    m_action->setAccessibleName(tip);
}

void ListRow::addLeading(QWidget* widget)
{
    widget->setParent(this);
    m_row->insertWidget(m_leading++, widget);
}

void ListRow::addTrailing(QWidget* widget, Qt::Alignment alignment)
{
    widget->setParent(this);
    const int at = m_action != nullptr ? m_row->indexOf(m_action) : m_row->count();
    m_row->insertWidget(at, widget, 0, alignment);
}

void ListRow::setSelected(bool selected)
{
    if (selected == m_selected) {
        return;
    }
    m_selected = selected;
    setProperty("selected", selected);
    for (QWidget* w : {static_cast<QWidget*>(this), static_cast<QWidget*>(m_title)}) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}

QString ListRow::title() const
{
    return m_title->text();
}

void ListRow::mousePressEvent(QMouseEvent* event)
{
    // The press is taken here, where the click is: passed on, a sheet would start moving its
    // window and the release would never come back (a click on a tile in a sheet did nothing).
    if (event->button() == Qt::LeftButton) {
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ListRow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        Q_EMIT clicked();
    }
    QWidget::mouseReleaseEvent(event);
}

void ListRow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        Q_EMIT clicked();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace pixelfx::ui
