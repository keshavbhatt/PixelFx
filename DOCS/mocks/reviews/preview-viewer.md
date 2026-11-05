# Design review: preview-viewer.html (large preview)

Reviewed 2026-11-05 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: see what a look or a batch recipe really does, at a size where it can be
judged, without leaving the task. Signature: one viewer for the look editor and Batch, with the
editor's own before and after, so nothing new has to be learned.

## Why one viewer

The look editor's preview and Batch's two 56 px pictures are too small to judge a look or a
recipe. Both need the same thing: the photo large, before and after, zoom. One viewer serves
both and keeps them alike; Batch adds Left and Right to go through every photo, so a recipe can
be checked on all of them, not only the first (the first photo is the one most often already
right).

## Measurements

- Top bar 50 px, as the window header; the title in 13 px semibold, the line under it 12 px
  muted: 7.2:1 dark, 7.1:1 light.
- Before, Split, After as a segmented control, the same as the Export formats; Fit and 1:1 and
  full screen as the editor's zoom bar.
- Photo arrows: 40 px round buttons, inside the 72 px side margins so they never cover the photo.
  Their icon is 12.5:1 (dark) and 17.4:1 (light) on the button; the button's own edge is faint
  on the canvas (1.5:1 dark, 1.0:1 light), so the icon carries the control, as for the editor's
  zoom bar.
- The counter "2 of 6" sits under the photo, centred, 12 px muted.

## Keyboard

Escape closes; Left and Right change photo (Batch); Space held shows Before; \ toggles the split;
Ctrl+0 fits, Ctrl+1 is 100%; F11 is full screen. All are the editor's keys for the same things.

## What it is not

It edits nothing: closing it leaves the look and the recipe as they were. The look editor's
preview stays where it is; the large preview is a second, bigger look at the same thing.

## Questions for the owner

- None. The entry points are added to `filter-editor.html` (a button in the preview's corner,
  and a double-click on it) and to `batch.html` ("View larger" beside the preview pictures, and
  a click on either picture).
