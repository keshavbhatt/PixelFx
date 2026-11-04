# Design review: watermark.html (Watermark)

Reviewed 2026-11-04 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good after one fix. Thesis: sign every exported photo the same way, whatever its shape.
Signature: size and margin as shares of the photo, previewed on portrait, square and the batch's
own photos, so one watermark fits a whole shoot.

## Blocking (fixed in this pass)

- **A fixed white mark disappears on light areas.** White text over the sample's pale sky
  measured 1.8:1; on the dark lake 13.8:1. A watermark has to read on every photo of a batch,
  so the colour choice gains **Auto** (light or dark per photo, from the area under the mark),
  now the default, with Soft shadow as a second guard.

## Improvements

- **Medium.** Position grid anchors are 26 px (24 px minimum met); the unselected border is
  3.4:1 (dark) and 3.3:1 (light) against the field, the selected fill 3.9:1 and 5.8:1.
- **Medium.** The three colour swatches need accessible names ("White", "Black", "Pick a
  colour") and the Auto pill a tooltip saying what it does (added in the mock).
- **Low.** Dragging the mark on the preview is pointer-only; the position grid and Margin are
  its keyboard twin.
- **Removed one thing.** "Rotate 45 degrees" only matters when the mark repeats across the
  photo; it now appears only when Repeat is on.

## What works

- Saved watermarks by name, shared by Export and Batch.
- Tokens ({year}, {camera}, {date}) fill per photo.
- The footer says plainly that the editor never shows the mark: no confusion about whether it
  was burned into the photo.
