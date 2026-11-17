# Design review: editor-panel-widths.html (S2, panel widths)

Reviewed 2026-11-17 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: let people give the room to what they use most, the looks or the sliders,
and keep it that way. Signature: the look tiles grow with the panel, so widening it is a bigger
preview of every look on the photo.

## Where it comes from

- FEATURES S2: persist window geometry and state, panel widths, last mode. Geometry and state are
  done; the last mode is dropped (owner, 2026-11-17: PixelFx opens on the welcome screen, as
  today). This mock is the panel widths.
- Today both editor panels have fixed widths: the Looks and Presets panel 272 px, the right
  panel (adjustments, Crop, Heal) 316 px. The tiles share the panel's width, two to a row
  (`tileGrid`), so they grow without new layout code; the sliders stretch already.

## Behaviour

- The 1 px border between a panel and the photo is the handle: an 8 px wide area centred on it.
  Over it the pointer is the horizontal resize arrow and a 2 px `--muted` line shows; the tooltip
  reads "Drag to change the width. Double-click: back to the usual width."
- Widths: the left panel 272 to 440 px, the right panel 316 to 440 px. The right panel keeps one
  width in Edit, Crop and Heal.
- A panel stops growing where the photo would get narrower than 480 px. A smaller window narrows
  the panels back towards their usual widths, never below them; the chosen width comes back when
  the window grows again. At the smallest window (1024 by 640) the photo has 380 px, as today.
- Both widths are remembered with the window's size and state. Batch keeps its own fixed panel.

## Measurements

- At the default window (1280 px): rail 56, left 272, right 316, photo 636. With the left panel
  at 360 px the photo has 548 px; at most the left panel reaches 428 px with the right at 316
  (photo 480).
- Tiles at 360 px: (360 - 2 x 10 padding - 8 gap) / 2 = 166 px wide, against 122 px today, a
  third larger.

## Lenses

1. **Accessibility.** The handle's line is `--muted` (7.0:1 dark, 6.8:1 light on `--panel`),
   wider than the 1 px border it replaces. The hit area is 8 px wide and the panel's height,
   above the 24 px minimum in its long axis; narrow targets are the desktop norm for splitters.
   The width is a preference with a good default, so it has no keyboard route; nothing becomes
   unreachable without it.
2. **Platform.** Dragging a pane's edge with the resize arrow, and double-click to reset, are what
   file managers and other editors do on every desktop.
3. **Visual craft.** No new chrome: the existing border becomes the handle. Not the accent,
   which means selection and change.
4. **Interaction.** The photo and the tiles reflow live while dragging; the previews redraw at
   their new size when the drag ends, so dragging stays smooth.
5. **Writing.** One tooltip, saying the drag and the reset.

## Then remove one thing

A visible grip (dots on the edge) was considered and left out: the pointer change says it, and a
grip would sit on every screen for something done once.

## Improvements

- Low: the Crop panel's chips are laid out for 316 px; at a wider panel the rows keep their
  breaks and leave room at the end. A wrapping layout can follow if it looks unfinished.

## What works

- One gesture, no setting; the default is today's layout.
