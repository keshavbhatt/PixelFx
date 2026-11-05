# Design review: editor-value-entry.html (typing a value, remembered groups)

Reviewed 2026-11-05 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: someone who knows the number they want types it, instead of nudging a
slider to land on it. Signature: the number itself is the field, so nothing is added to the
panel and the row keeps its height.

## Where it comes from

- **Lightroom Classic** turns the number into a field on a click; Enter keeps it, Up and Down
  nudge it, Shift nudges further. The mock follows it.
- **darktable** types a value after a right-click on the slider. A right-click is hard to find
  and is the context menu everywhere else in PixelFx, so the mock takes Lightroom's click.
- **Lightroom and darktable** both keep the open or closed state of their panels between
  sessions; neither keeps it per photo.

## Measurements

- The open field: 56 x 20 px in the value's place, so the row does not move. Its edge in the
  accent is 6.9:1 on the panel (dark) and 5.3:1 (light); the typed text 15.6:1 and 17.4:1.
  The 2 px focus ring is the same as every other field.
- The value under the pointer: a 1 px box in `--border`, 1.3:1 (dark) and 1.4:1 (light). It is
  a hint, not the only cue: the text cursor and the tooltip "Click to type a value" say the same,
  and the keyboard path (Enter on a focused slider) does not need it.
- Target: the value is 12 px text, about 36 x 16 px with its box; it is a secondary way in, the
  24 px slider stays the main control.

## Behaviour to keep

- Typed values use the slider's unit and accept "0.35", "+0.35" and "-1"; a comma as the
  decimal mark works where the locale uses one. Past the end goes to the end. Anything that is
  not a number leaves the value as it was.
- One history step for a typed value, the same as one drag.
- Escape on the field only closes the field; it does not leave a mode or close a sheet.

## Questions for the owner

- None. Both behaviours follow Lightroom; the remembered groups are a setting of the app, not
  a change to any file format in `DOCS/presets.md`.
