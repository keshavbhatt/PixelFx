# Design review: heal.html (Spot heal)

Reviewed 2026-11-04 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: make a blemish disappear with one click, and change your mind later.
Signature: every spot stays a live object (healed area solid, source hollow, joined by a dashed
line) that can be moved, resized or deleted, not a pixel change.

## Blocking

None. Measured:
- Spot rings are white with a 1 px dark outline: white alone is 1.4:1 on a pale sky, the dark
  outline is 15:1 there, so the ring reads on any photo (3:1 needed for non-text).
- Panel text and switches use tokens already measured in `editor.md`.

## Improvements

- **Medium.** Adding a spot needs a pointer. Accepted for a brush tool (no keyboard model for
  painting exists in comparable apps); everything after placing is reachable by keyboard: the
  Spots list selects, Delete removes, [ and ] resize, arrow keys nudge the selected spot.
- **Medium.** Show spots moved from H to V: H is History everywhere else (`shortcuts.html`).
- **Low.** "Reveal dust" needs a one-line explanation the first time it is switched on.
- **Removed one thing.** The help sentence at the top of the panel repeated the hint strip under
  the photo; it is gone, and the Spot and Brush buttons carry the explanation in their tooltips.

## What works

- Heal is its own mode on the rail (R), so the canvas can take clicks without fighting pan and
  crop gestures; leaving it is Escape or Done.
- Sizes relative to the photo, so a spot looks the same at every zoom and on export.
- Spots never travel in presets or batch: they belong to one photo, which matches how people
  think about blemishes.
