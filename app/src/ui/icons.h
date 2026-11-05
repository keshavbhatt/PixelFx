#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

class QLabel;

// Themed monochrome icons (DESIGN.md): the SVGs under :/icons/ui are drawn in
// #000000 and tinted at runtime, so one asset serves both schemes and the
// accent/muted variants. Rendered pixmaps are cached per (name, colour, size, dpr).
namespace pixelfx::ui::icons {

/// A QIcon whose normal/disabled modes are tinted `color` / `disabledColor`; a colour of the
/// theme's tokens follows a theme switch.
[[nodiscard]] QIcon themed(const QString& name, const QColor& color, const QColor& disabledColor = {});
/// A single pixmap at `size` logical px for the given device pixel ratio.
[[nodiscard]] QPixmap pixmap(const QString& name, const QColor& color, int size,
                             qreal devicePixelRatio = 1.0);
/// The brand mark as an icon (multi-size, from :/icons/pixelfx.png).
/// A themed icon that shows in `onColor` while its button is checked.
[[nodiscard]] QIcon toggle(const QString& name, const QColor& color, const QColor& onColor,
                           const QColor& disabledColor = {});
[[nodiscard]] QIcon brand();
/// Shows the glyph in `label`, remembering it so a theme switch draws it again in the new
/// theme's colour.
void setGlyph(QLabel* label, const QString& name, const QColor& color, int size);
/// Path of a tinted copy of the SVG on disk, for style-sheet url() references.
[[nodiscard]] QString tintedFile(const QString& name, const QColor& color);

} // namespace pixelfx::ui::icons
