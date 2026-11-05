#include "ui/flow_layout.h"

#include <QWidget>

#include <algorithm>

namespace pixelfx::ui {

FlowLayout::FlowLayout(QWidget* parent, int spacing)
    : QLayout(parent)
    , m_spacing(spacing)
{}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem* item = takeAt(0)) {
        delete item;
    }
}

void FlowLayout::addItem(QLayoutItem* item)
{
    m_items.append(item);
}

QLayoutItem* FlowLayout::itemAt(int index) const
{
    return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
}

QLayoutItem* FlowLayout::takeAt(int index)
{
    return index >= 0 && index < m_items.size() ? m_items.takeAt(index) : nullptr;
}

int FlowLayout::heightForWidth(int width) const
{
    return arrange(QRect(0, 0, width, 0), false);
}

void FlowLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    static_cast<void>(arrange(rect, true));
}

QSize FlowLayout::sizeHint() const
{
    return minimumSize();
}

QSize FlowLayout::minimumSize() const
{
    QSize size;
    for (const QLayoutItem* item : m_items) {
        size = size.expandedTo(item->minimumSize());
    }
    const QMargins m = contentsMargins();
    return size + QSize(m.left() + m.right(), m.top() + m.bottom());
}

int FlowLayout::arrange(const QRect& rect, bool apply) const
{
    const QRect area = rect.marginsRemoved(contentsMargins());
    int x = area.x();
    int y = area.y();
    int rowHeight = 0;
    for (QLayoutItem* item : m_items) {
        if (item->widget() != nullptr && item->widget()->isHidden()) {
            continue;
        }
        const QSize size = item->sizeHint();
        if (x > area.x() && x + size.width() > area.right() + 1) {
            x = area.x(); // wrap
            y += rowHeight + m_spacing;
            rowHeight = 0;
        }
        if (apply) {
            item->setGeometry(QRect(QPoint(x, y), size));
        }
        x += size.width() + m_spacing;
        rowHeight = std::max(rowHeight, size.height());
    }
    const QMargins m = contentsMargins();
    return (y + rowHeight) - rect.y() + m.bottom();
}

} // namespace pixelfx::ui
