#include "ui/step_list.h"

#include "ui/icons.h"
#include "ui/list_row.h"
#include "ui/style.h"

#include <QCheckBox>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

StepList::StepList(QWidget* parent)
    : QWidget(parent)
    , m_column(new QVBoxLayout(this))
{
    m_column->setContentsMargins(6, 0, 6, 0);
    m_column->setSpacing(kGapTight);
    m_column->addStretch();
}

StepList::~StepList() = default;

void StepList::setRows(const QList<Row>& rows)
{
    for (const Entry& e : std::as_const(m_rows)) {
        e.row->hide();
        e.row->deleteLater(); // it may be the one being clicked
    }
    m_rows.clear();
    for (const Row& r : rows) {
        auto* row = new ListRow(this);
        auto* handle = new QLabel(row);
        handle->setPixmap(icons::pixmap(u"grip-vertical"_s, tokens().faint, 14, devicePixelRatioF()));
        handle->setCursor(Qt::SizeVerCursor);
        handle->setToolTip(tr("Drag to reorder"));
        handle->installEventFilter(this);
        auto* check = new QCheckBox(row);
        check->setChecked(r.on);
        check->setToolTip(tr("Switch the step on or off"));
        check->setAccessibleName(tr("%1 on").arg(r.title));
        row->addLeading(handle);
        row->addLeading(check);
        row->setIcon(r.glyph);
        row->setTitle(r.title);
        row->setSubtitle(r.subtitle);
        row->setAction(u"more"_s, tr("Move, duplicate or remove"));
        connect(row, &ListRow::clicked, this, [this, row] { setCurrent(indexOf(row)); });
        connect(check, &QCheckBox::toggled, this,
                [this, check](bool on) { Q_EMIT toggled(indexOf(check), on); });
        connect(row, &ListRow::actionClicked, this, [this, row] {
            const QToolButton* button = row->actionButton();
            Q_EMIT menuRequested(indexOf(row), button->mapToGlobal(QPoint(0, button->height())));
        });
        m_column->insertWidget(m_column->count() - 1, row); // before the stretch
        m_rows.append({row, handle, check});
    }
    m_current = -1;
}

void StepList::setSubtitle(int row, const QString& subtitle)
{
    if (row >= 0 && row < count()) {
        m_rows.at(row).row->setSubtitle(subtitle);
    }
}

void StepList::setCurrent(int row)
{
    m_current = row >= 0 && row < count() ? row : -1;
    for (int i = 0; i < count(); ++i) {
        m_rows.at(i).row->setSelected(i == m_current);
    }
    Q_EMIT currentChanged(m_current);
}

bool StepList::isOn(int row) const
{
    return row >= 0 && row < count() && m_rows.at(row).check->isChecked();
}

void StepList::setOn(int row, bool on)
{
    if (row >= 0 && row < count()) {
        m_rows.at(row).check->setChecked(on);
    }
}

void StepList::moveRow(int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= count() || to >= count()) {
        return;
    }
    place(from, to);
    Q_EMIT moved(from, to);
}

int StepList::indexOf(const QObject* part) const
{
    for (int i = 0; i < count(); ++i) {
        const Entry& e = m_rows.at(i);
        if (part == e.row || part == e.handle || part == e.check) {
            return i;
        }
    }
    return -1;
}

void StepList::place(int from, int to)
{
    const Entry entry = m_rows.takeAt(from);
    m_rows.insert(to, entry);
    m_column->removeWidget(entry.row);
    m_column->insertWidget(to, entry.row);
    if (m_current == from) {
        m_current = to;
    } else if (m_current >= std::min(from, to) && m_current <= std::max(from, to)) {
        m_current += from < to ? -1 : 1;
    }
}

bool StepList::eventFilter(QObject* watched, QEvent* event)
{
    // A drag on a handle moves its row to where the pointer is.
    const int index = indexOf(watched);
    if (index < 0 || watched != m_rows.at(index).handle) {
        return QWidget::eventFilter(watched, event);
    }
    const auto* mouse = static_cast<QMouseEvent*>(event);
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        if (mouse->button() == Qt::LeftButton) {
            m_dragFrom = index;
            m_dragAt = index;
            return true;
        }
        break;
    case QEvent::MouseMove:
        if (m_dragFrom >= 0) {
            const int y = mapFromGlobal(mouse->globalPosition().toPoint()).y();
            int target = 0;
            for (int i = 0; i < count(); ++i) {
                if (y > m_rows.at(i).row->geometry().center().y()) {
                    target = i;
                }
            }
            if (target != m_dragAt) {
                place(m_dragAt, target);
                m_dragAt = target;
            }
            return true;
        }
        break;
    case QEvent::MouseButtonRelease:
        if (m_dragFrom >= 0) {
            const int from = m_dragFrom;
            m_dragFrom = -1;
            if (m_dragAt != from) {
                Q_EMIT moved(from, m_dragAt);
            }
            return true;
        }
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace pixelfx::ui
