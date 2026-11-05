#include "ui/size_list.h"

#include "ui/icons.h"
#include "ui/style.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QPainter>
#include <QStandardItemModel>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kNameRole = Qt::UserRole + 20;
constexpr int kDetailRole = Qt::UserRole + 21;
constexpr int kKindRole = Qt::UserRole + 22;
enum Kind
{
    Row = 0,
    Heading = 1,
    Line = 2,
};

constexpr int kRowHeight = 30;
constexpr int kHeadingHeight = 24;
constexpr int kLineHeight = 9;
constexpr int kSide = 10;
constexpr int kTick = 15;

QStandardItem* lastItem(const QComboBox* combo)
{
    const auto* model = qobject_cast<const QStandardItemModel*>(combo->model());
    return model != nullptr ? model->item(combo->count() - 1) : nullptr;
}

} // namespace

namespace sizelist {

void install(QComboBox* combo)
{
    combo->setItemDelegate(new SizeListDelegate(combo));
    combo->view()->setMinimumWidth(370);
    combo->setMaxVisibleItems(16);
}

void addHeading(QComboBox* combo, const QString& text)
{
    combo->addItem(text);
    if (QStandardItem* item = lastItem(combo)) {
        item->setData(Heading, kKindRole);
        item->setFlags(Qt::NoItemFlags); // a heading, not a choice
    }
}

void addItem(QComboBox* combo, const QString& name, const QString& detail, const QVariant& data)
{
    combo->addItem(detail.isEmpty() ? name : u"%1 · %2"_s.arg(name, detail), data);
    combo->setItemData(combo->count() - 1, name, kNameRole);
    combo->setItemData(combo->count() - 1, detail, kDetailRole);
}

void addSeparator(QComboBox* combo)
{
    combo->addItem(QString());
    if (QStandardItem* item = lastItem(combo)) {
        item->setData(Line, kKindRole);
        item->setFlags(Qt::NoItemFlags);
    }
}

QString nameOf(const QComboBox* combo, int index)
{
    return combo->itemData(index, kNameRole).toString();
}

void setItem(QComboBox* combo, int index, const QString& name, const QString& detail)
{
    combo->setItemText(index, detail.isEmpty() ? name : u"%1 · %2"_s.arg(name, detail));
    combo->setItemData(index, name, kNameRole);
    combo->setItemData(index, detail, kDetailRole);
}

} // namespace sizelist

SizeListDelegate::SizeListDelegate(QComboBox* combo)
    : QStyledItemDelegate(combo)
    , m_combo(combo)
{}

SizeListDelegate::~SizeListDelegate() = default;

QSize SizeListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const int kind = index.data(kKindRole).toInt();
    int height = kRowHeight;
    if (kind == Heading) {
        height = kHeadingHeight;
    } else if (kind == Line) {
        height = kLineHeight;
    }
    const QFontMetrics fm(option.font);
    const int width = (kSide * 2) + kTick + kSide + fm.horizontalAdvance(index.data(kNameRole).toString()) +
                      24 + fm.horizontalAdvance(index.data(kDetailRole).toString());
    return {width, height};
}

void SizeListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    const Tokens& t = tokens();
    const QRect r = option.rect;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    switch (index.data(kKindRole).toInt()) {
    case Heading: {
        QFont small = option.font;
        small.setPixelSize(11);
        small.setWeight(QFont::DemiBold);
        painter->setFont(small);
        painter->setPen(t.muted);
        painter->drawText(r.adjusted(kSide, 6, -kSide, -2), Qt::AlignLeft | Qt::AlignBottom,
                          index.data().toString());
        break;
    }
    case Line:
        painter->setPen(QPen(t.border, 1.0));
        painter->drawLine(QPointF(r.left() + 4, r.center().y() + 0.5),
                          QPointF(r.right() - 4, r.center().y() + 0.5));
        break;
    default: {
        if ((option.state & (QStyle::State_MouseOver | QStyle::State_Selected)) != 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(t.hover);
            painter->drawRoundedRect(QRectF(r).adjusted(0, 1, 0, -1), 6, 6);
        }
        if (index.row() == m_combo->currentIndex()) {
            const qreal ratio = painter->device()->devicePixelRatioF();
            const QPixmap tick = icons::pixmap(u"check"_s, t.accent, 14, ratio);
            painter->drawPixmap(QPointF(r.left() + kSide, r.center().y() - 7.0), tick);
        }
        const QRect text = r.adjusted(kSide + kTick + kSide, 0, -kSide, 0);
        QString name = index.data(kNameRole).toString();
        if (name.isEmpty()) {
            name = index.data().toString();
        }
        painter->setFont(option.font);
        painter->setPen(t.text);
        painter->drawText(text, Qt::AlignLeft | Qt::AlignVCenter, name);
        QFont small = option.font;
        small.setPixelSize(12);
        painter->setFont(small);
        painter->setPen(t.muted);
        painter->drawText(text, Qt::AlignRight | Qt::AlignVCenter, index.data(kDetailRole).toString());
        break;
    }
    }
    painter->restore();
}

} // namespace pixelfx::ui
