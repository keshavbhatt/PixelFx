#include "ui/menu_style.h"

#include "ui/icons.h"
#include "ui/style.h"

#include <QAction>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionMenuItem>
#include <QWidget>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pixelfx::ui::menu_style {

namespace {

constexpr int kPanelPadding = 5; // the panel's border to its rows
constexpr int kPanelBorder = 1;
constexpr int kRadius = 10;   // the panel's corners
constexpr int kRowRadius = 6; // a highlighted row's corners
constexpr int kRowPaddingX = 10;
constexpr int kRowPaddingY = 6;
constexpr int kIcon = 16;
constexpr int kLine = 19;        // a row's line, as the mocks' 13 px text on a 1.45 line height
constexpr int kShortcutGap = 24; // the least space between a row's text and its shortcut
constexpr int kEndMark = 14;     // a submenu's chevron or a check
constexpr int kSeparator = 11;   // its height: 5 px either side of a 1 px line
constexpr int kSubmenuGap = 4;   // between a submenu and its parent

const QStyleOptionMenuItem* menuItem(const QStyleOption* option)
{
    return qstyleoption_cast<const QStyleOptionMenuItem*>(option);
}

// Whether the row keeps the end column for a chevron or a check (every row of a menu that has
// checkable items keeps it, so their shortcuts line up).
bool hasEndColumn(const QStyleOptionMenuItem& item)
{
    return item.menuItemType == QStyleOptionMenuItem::SubMenu || item.menuHasCheckableItems;
}

QFont shortcutFont(const QFont& base)
{
    QFont font = base;
    font.setPixelSize(12);
    return font;
}

} // namespace

bool drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                   const QWidget* widget)
{
    if (element != QStyle::PE_PanelMenu && element != QStyle::PE_FrameMenu) {
        return false;
    }
    const Tokens& t = tokens();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRectF area =
        QRectF(widget != nullptr ? widget->rect() : option->rect).adjusted(0.5, 0.5, -0.5, -0.5);
    if (element == QStyle::PE_PanelMenu) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(t.elevated);
    } else {
        painter->setPen(QPen(t.border, 1.0));
        painter->setBrush(Qt::NoBrush);
    }
    painter->drawRoundedRect(area, kRadius, kRadius);
    painter->restore();
    return true;
}

namespace {

// What a row draws with: its state and where its parts go.
struct Row
{
    const QStyleOptionMenuItem& item;
    QPainter* painter;
    qreal dpr;
    bool enabled;
    bool checked;
    int middle;
};

// The icon column: kept for every row when the menu has icons; a checked row's icon in the
// accent. Returns where the text starts.
int drawIconColumn(const Row& row, int left)
{
    if (row.item.maxIconWidth <= 0) {
        return left;
    }
    if (!row.item.icon.isNull()) {
        QPixmap glyph =
            row.item.icon.pixmap(QSize(kIcon, kIcon), row.dpr, row.enabled ? QIcon::Normal : QIcon::Disabled);
        if (row.checked && row.enabled) {
            QPainter tint(&glyph);
            tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
            tint.fillRect(glyph.rect(), tokens().accent);
        }
        row.painter->drawPixmap(left, row.middle - (kIcon / 2), glyph);
    }
    return left + kIcon + kGapItems;
}

// The end column: a submenu's chevron, or a check for a checked row. Returns where the text ends.
int drawEndColumn(const Row& row, int right)
{
    if (!hasEndColumn(row.item)) {
        return right;
    }
    const Tokens& t = tokens();
    QString glyph;
    QColor colour = row.enabled ? t.muted : t.faint;
    if (row.item.menuItemType == QStyleOptionMenuItem::SubMenu) {
        glyph = u"chevron-right"_s;
    } else if (row.checked) {
        glyph = u"check"_s;
        colour = row.enabled ? t.accent : t.faint;
    }
    if (!glyph.isEmpty()) {
        row.painter->drawPixmap(right - kEndMark + 1, row.middle - (kEndMark / 2),
                                icons::pixmap(glyph, colour, kEndMark, row.dpr));
    }
    return right - kEndMark - kGapItems;
}

// The text, and the shortcut after a tab, at the right end in muted type.
void drawTexts(const Row& row, const QRect& area)
{
    const Tokens& t = tokens();
    QString text = row.item.text;
    QString shortcut;
    if (const qsizetype tab = text.indexOf(u'\t'); tab >= 0) {
        shortcut = text.mid(tab + 1);
        text = text.left(tab);
    }
    if (!shortcut.isEmpty()) {
        row.painter->setFont(shortcutFont(row.item.font));
        row.painter->setPen(row.enabled ? t.muted : t.faint);
        row.painter->drawText(
            area, Qt::AlignVCenter | Qt::AlignRight | Qt::TextHideMnemonic | Qt::TextSingleLine, shortcut);
    }
    row.painter->setFont(row.item.font);
    row.painter->setPen(row.enabled ? t.text : t.faint);
    row.painter->drawText(area, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextHideMnemonic | Qt::TextSingleLine,
                          text);
}

} // namespace

bool drawItem(const QStyleOption* option, QPainter* painter, const QWidget* widget)
{
    const QStyleOptionMenuItem* item = menuItem(option);
    if (item == nullptr) {
        return false;
    }
    // Rows run to the panel's border (Wayland opens a submenu at its row's edge); the panel's
    // padding is drawn inside them.
    const QRect rect = item->rect.adjusted(kPanelPadding, 0, -kPanelPadding, 0);
    if (item->menuItemType == QStyleOptionMenuItem::Separator) {
        painter->fillRect(QRect(rect.left() + 4, rect.center().y(), rect.width() - 8, 1), tokens().border);
        return true;
    }
    const bool row = item->menuItemType == QStyleOptionMenuItem::Normal ||
                     item->menuItemType == QStyleOptionMenuItem::SubMenu ||
                     item->menuItemType == QStyleOptionMenuItem::DefaultItem;
    if (!row) {
        return true; // margins and empty areas show the panel
    }
    const bool enabled = item->state.testFlag(QStyle::State_Enabled);
    const Row parts{*item,
                    painter,
                    widget != nullptr ? widget->devicePixelRatioF() : 1.0,
                    enabled,
                    item->checkType != QStyleOptionMenuItem::NotCheckable && item->checked,
                    rect.center().y()};
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (enabled && item->state.testFlag(QStyle::State_Selected)) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(tokens().hover);
        painter->drawRoundedRect(QRectF(rect), kRowRadius, kRowRadius);
    }
    const int left = drawIconColumn(parts, rect.left() + kRowPaddingX);
    const int right = drawEndColumn(parts, rect.right() - kRowPaddingX);
    drawTexts(parts, QRect(left, rect.top(), right - left + 1, rect.height()));
    painter->restore();
    return true;
}

std::optional<QSize> itemSize(const QStyleOption* option, const QSize& contents)
{
    const QStyleOptionMenuItem* item = menuItem(option);
    if (item == nullptr) {
        return std::nullopt;
    }
    if (item->menuItemType == QStyleOptionMenuItem::Separator) {
        return QSize(contents.width(), kSeparator);
    }
    int width = (2 * (kPanelPadding + kRowPaddingX)) + contents.width();
    if (item->maxIconWidth > 0) {
        width += kIcon + kGapItems;
    }
    if (item->reservedShortcutWidth > 0) {
        width += kShortcutGap; // QMenu adds the shortcut's own width
    }
    if (hasEndColumn(*item)) {
        width += kEndMark + kGapItems;
    }
    const int height = std::max({contents.height(), kIcon, kLine}) + (2 * kRowPaddingY);
    return QSize(width, height);
}

void placeSubmenu(QMenu* menu)
{
    // Qt opens a submenu that does not fit on the right at its parent's row edge, ignoring
    // PM_SubMenuOverlap, so the left side is placed here.
    const QMenu* parent = nullptr;
    for (QObject* owner : menu->menuAction()->associatedObjects()) {
        if (const auto* m = qobject_cast<const QMenu*>(owner); m != nullptr && m != menu && m->isVisible()) {
            parent = m;
        }
    }
    if (parent == nullptr) {
        return;
    }
    const QRect outer = parent->geometry();
    QRect placed = menu->geometry();
    if (placed.left() > outer.right() || placed.right() < outer.left()) {
        return; // already clear of it
    }
    if (placed.center().x() < outer.center().x()) {
        placed.moveRight(outer.left() - kSubmenuGap - 1);
    } else {
        placed.moveLeft(outer.right() + kSubmenuGap + 1);
    }
    menu->move(placed.topLeft());
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric)
{
    switch (metric) {
    case QStyle::PM_MenuPanelWidth:
        return kPanelBorder;
    case QStyle::PM_MenuHMargin:
        return 0; // drawn inside each row, see drawItem
    case QStyle::PM_MenuVMargin:
        return kPanelPadding;
    case QStyle::PM_SubMenuOverlap:
        // Qt places a submenu this far past the edge of its row: past the parent's border, then a gap.
        return kPanelBorder + kSubmenuGap;
    default:
        return std::nullopt;
    }
}

} // namespace pixelfx::ui::menu_style
