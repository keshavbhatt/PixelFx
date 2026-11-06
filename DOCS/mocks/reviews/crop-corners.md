# Design review: crop-corners.html (Fix by four corners)

Reviewed 2026-11-06 with the method in `DESIGN.md` section 5, before the owner's review. Two
files: `crop-corners.html` (placing the corners) and `crop-corners-done.html` (after applying).

## Summary

Rating: Good. Thesis: make something that should be square, square, by showing where its
corners are. Signature: the magnifier at the dragged corner, so the corner lands on the exact
spot.

## Where it comes from

- `crop.html` (approved 2026-11-04) has the button "Fix by four corners" under Perspective and
  the note "drag four corners onto a rectangle (keystone correction for documents and
  buildings)"; it never drew what happens after the click. FEATURES G5: the vertical and
  horizontal sliders are built, the four-corner fix is the part left.
- `DOCS/presets.md` already reserves `geometry.perspective.corners` (null today).
- Convention: document scanners (a page squared from its four corners) and photo editors'
  four-point perspective tools (a building or a painting squared, the rest of the photo kept).
  The two jobs need two results, so Keep chooses between them.
- The built pieces it joins: Crop mode (the panel, the footer, Enter and Escape), the crop
  frame and its thirds, the shared segmented control, the dimmed outside of the crop.

## Measurements

- Handles: 18 px white discs with a 2 px ring of black at 55%, on each corner of the shape;
  the selected one has a 4 px `--accent` ring at 28 px across. The target is 32 by 32 px.
- The shape: a 2 px white outline, thirds lines at 1 px white 45% that follow its sides, the
  photo outside it dimmed by black at 45% (the crop's dimming).
- Magnifier: 156 px across, three times the photo under the pointer, a 3 px white ring and a
  `--accent` cross of 28 px in its middle; it sits beside the corner being dragged, on the side
  towards the photo's middle, so it never leaves the canvas or covers the corner.
- Panel: the two sliders dimmed to 45% (the app's off sliders) with the tooltip "Off while four
  corners set the perspective"; "Fix by four corners" as a 26 px small button, on (`--accent` on
  `--accentSoft`) while placing; Reset corners a link at the right end of that row; "Keep" 12 px
  `--muted`, 10 px under it, then Whole photo | Inside the corners, the shared segmented control
  across the panel.
- After applying: the button reads "Edit the four corners", the link "Remove".
- The strip under the photo says what to do in one line, 13 px `--muted`, as Crop's does.
- Contrast: the white handle and outline on the dimmed photo are at least 3:1 against any
  photo colour under them thanks to the black ring; `--accent` on `--accentSoft` 6.0:1 dark,
  4.9:1 light; the link 6.9:1 and 5.3:1; `--muted` on `--panel` 7.0:1 and 6.8:1.

## Accessibility

- From the keyboard: Tab goes through the four corners (named "Top left corner" and so on), the
  arrow keys move the selected corner by one pixel of the screen, Shift by ten; Enter applies,
  Escape leaves the corners as they were. Nothing needs the pointer, only a pointer is quicker.
- Each control has its name: the button and the link say what they do; the segmented control's
  two choices are words.
- Nothing by colour alone: the selected corner also carries the magnifier while dragged, and its
  ring is a shape, not only a hue.

## Platform conventions

- Reached from Crop (C) and its panel, like every other crop tool; Enter and Escape as Crop's
  own; one undo step for applying, one for Remove, one for each change of Keep.
- Reset in the footer still resets the whole crop, the corners with it.

## Interaction

- The handles start on the photo's own corners, so the first drag is always onto the shape.
- A corner cannot cross the others' lines (the shape stays convex) nor come closer than 2% of
  the photo's long edge to another, so no drag makes a picture that cannot be computed.
- Keep: Whole photo turns the whole picture so the shape is square and trims the empty
  corners to the largest rectangle inside, the size of the photo as near as it allows; Inside
  the corners keeps only the shape, squared, its width and height from the shape's average
  sides. Switching Keep after applying needs no new corners.
- The vertical and horizontal sliders are off while corners are set: two ways of setting one
  perspective would fight, and the corners say more.
- The picture updates when a corner is let go, not during the drag: the outline, the thirds and
  the magnifier follow the pointer at once, the warp takes a moment on a large photo.

## Writing

- One name: "Fix by four corners" to start, "Edit the four corners" to come back, "Remove" to
  take it off, "Reset corners" to start again. "Keep: Whole photo / Inside the corners" says what
  stays.
- The strip: "Drag each corner onto a corner of something that should be square: a page, a
  window, a wall. Enter applies, Escape cancels."

## What building it takes

- libpixelfx: a projective warp (the shape onto a rectangle) in the one geometry resampling,
  after straightening and before the crop; Whole photo's largest rectangle inside the warped
  photo; the output size of each. Tests: a drawn square photographed at an angle comes back
  square; the export matches the preview; Keep and Remove.
- The file format: `geometry.perspective.corners` holds the four corners as fractions of the
  straightened photo (`topLeft`, `topRight`, `bottomRight`, `bottomLeft`, each `x` and `y`) and
  `geometry.perspective.keep` is `"whole"` or `"inside"`; the recipe version goes up by one, with
  a migration (an older file has no corners) and a test, as `DOCS/presets.md` asks.
- The app: the corner overlay on the canvas with the magnifier, the panel's rows, the keys.

## Remove one thing

Removed: a line under Keep repeating that the sliders are off while the corners are set; the
dimmed sliders show it and their tooltip says why.

## Questions for the owner

- Approve Fix by four corners as drawn: the handles and the magnifier, Keep, the sliders off
  while corners are set, Edit the four corners and Remove after applying.
- Keep both results (recommended: a page wants Inside the corners, a building wants Whole
  photo), or only one of them?
- Which should Keep start on? Whole photo (recommended: nothing of the photo is lost until
  asked) or Inside the corners (fewer steps for a page)?
