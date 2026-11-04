# Design review: editor.html (Editor, Looks)

Reviewed 2026-11-04 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good after the fixes below. Thesis: make this photo better while watching it.
Signature: every look tile is the user's own photo with that look, so choosing a filter is
choosing between pictures, not names. Chrome is neutral and the photo carries the colour.

## Blocking (fixed in this pass)

- **Hint text contrast.** `faint` measured 3.3:1 (dark) and 3.0:1 (light) on `panel` for 12 px
  text; needs 4.5:1. Now `#8E8E8E` / `#676767`: 5.1:1 and 5.2:1.
- **Light accent.** Changed slider values in `accent` measured 4.3:1 on `panel` and 4.0:1 on
  `accentSoft`. Now `#2560CC`: 5.3:1 and 4.9:1; white on the primary button 5.8:1.
- **Slider tracks and switches.** `track` measured 1.5:1 against `panel`; non-text controls
  need 3:1. Now `#696969` / `#8E8E8E`: 3.0:1.
- **Hit targets.** Colour-mixer swatches were 20 px and slider rows 18 px high; both now 24 px.
- **Menu coverage.** Undo, Redo, Copy and Paste settings, Auto, Reset, Before and after, zoom
  and Hide panels were reachable only from buttons or keys. The main menu gains Edit, View and
  Photo submenus (`menu.html`).

## Improvements

- **Medium.** The right panel's "Copy" could be read as copying the image. In the app the
  button reads "Copy settings" when the panel is wide enough and always has that tooltip and
  accessible name.
- **Medium.** Rail labels were 10 px; now 11 px, the desktop minimum in section 5.
- **Low.** The look strength slider sits at the bottom of the left panel, away from the tile
  just clicked. Keep it there (it stays put while the grid scrolls), but flash its row once when
  a look is applied so the eye finds it.
- **Removed one thing.** The "Edited" pill beside the file name repeated the filmstrip's edited
  mark and the History button; it is gone.

## What works

- Groups in a photographer's order (Light, Color, Color mixer, Curves, Detail, Effects,
  Geometry), collapsed when untouched: progressive disclosure without hiding what changed.
- A changed value is shown three ways (accent number, brighter label, group dot), so colour is
  never the only signal.
- Meaningful slider tracks for Temperature, Tint and Saturation.
- One primary-looking action in the header (Export); everything else is quiet.

## Platform notes

Linux and Windows: header menu, no menu bar. macOS: the same entries in the native menu bar,
Settings under the app menu with Cmd+, (FEATURES S14).
