# Design review: editor-looks-preview.html (Looks, hover preview)

Reviewed 2026-11-05 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: see a look large on the photo before choosing it, without a history step
for each one tried. Signature: the Looks tab previews exactly as the Presets tab does, so both
tabs browse the same way.

## Behaviour

- The hover preview of the approved catalog (`editor-presets-catalog.md`, "Hover preview,
  reworked"), unchanged: 0.35 s resting on a tile, 0.15 s once a preview shows, the bar filling
  along the bottom of the tile's picture, the Preview tag and the line at the top. Escape, a
  click or leaving the tiles ends it; zoomed in it is drawn sharp, with the loading chip.
- The look shows at its own strength over the photo's edit, in place of the applied look:
  exactly what a click gives, so the preview never surprises.
- Original previews too: the photo without any look.
- One shared piece runs the timing for both tabs (the app keeps one copy of the behaviour).

## Added

- **"Applied" above Look strength**, naming the look the slider belongs to ("Applied Warm"), as
  the Presets tab names its preset above Amount. During a preview it says that the applied look
  has not changed; with no look it reads "Applied None" and the slider is greyed. Text: `--muted`
  label 7.0:1 dark and 6.8:1 light, the name in `--text`.

## Measurements

- The bar on the one-line tile sits along the picture's bottom edge, as on preset tiles; the
  accent on its 55% black track, at least 3:1 over any picture.
- The tag and the line are the ones measured in `editor-presets-catalog.md`.

## Remove one thing

The screen is already lean: the preview adds no control, only a state of what is there.

## Questions for the owner

- None.
