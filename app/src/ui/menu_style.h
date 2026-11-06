#pragma once

#include <QSize>
#include <QStyle>

#include <optional>

class QMenu;
class QPainter;
class QStyleOption;
class QWidget;

namespace pixelfx::ui::menu_style {

// Menus drawn by the app: a rounded panel, rows with an icon column, the
// text, the shortcut at the right end in muted type, a chevron for a submenu, and a check for a
// checked item. PixelFxStyle hands menu elements here; each function returns nothing (or false)
// for anything that is not a menu's.

/// Draws the menu's panel or frame.
bool drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                   const QWidget* widget);
/// Draws one row of a menu.
bool drawItem(const QStyleOption* option, QPainter* painter, const QWidget* widget);
/// The size of a row whose text measures `contents` (QMenu adds the widest shortcut).
[[nodiscard]] std::optional<QSize> itemSize(const QStyleOption* option, const QSize& contents);
[[nodiscard]] std::optional<int> pixelMetric(QStyle::PixelMetric metric);
/// Keeps a submenu that is being shown clear of its parent menu, on whichever side it opens.
void placeSubmenu(QMenu* menu);

} // namespace pixelfx::ui::menu_style
