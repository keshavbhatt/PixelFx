# Design review: editor-develop.html (every Lightroom setting)

Reviewed 2026-11-05 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good after two fixes. Thesis: every setting a Lightroom preset uses can be seen and
changed, without making the panel longer for someone who never needs the finer ones.
Signature: one "More" per group for the finer settings, and the four colour grading ranges at a
glance as small wheels, edited on one large wheel.

## What changes, and where it comes from

- **Treatment** (Color or Black & white) at the top of Color, with V, as in Lightroom's Basic
  panel. In black and white the Color mixer becomes the **B&W mix** (Lightroom's B&W panel):
  same swatches, one slider per colour. Vibrance and Saturation are greyed, not hidden, so the
  panel does not jump and their values return with colour.
- **Profile**: Standard or Vivid, what Lightroom's Adobe Color and Adobe Vivid stand for.
- **Color grading** reopens the approved two-wheel mock (owner, 2026-11-05: add midtones and
  global). Four 44 px wheels pick the range and show each one's colour; a 120 px wheel edits the
  picked range, with its Luminance; Blending and Balance act on all. Lightroom's three-way view
  puts three wheels side by side; four wheels side by side would be 64 px each in this 288 px
  column, too small to aim at, so the mock follows Lightroom's single-wheel view instead, with
  the small wheels as its range chooser.
- **Curves**: Points (today's curve) or Regions (Lightroom's parametric curve: Highlights,
  Lights, Darks, Shadows, and three marks under the graph for the splits).
- **Detail** gains Texture first, with Clarity and Dehaze (Lightroom's order). **More** opens
  sharpening radius, detail and masking, and the detail, contrast and smoothness of noise
  reduction.
- **Effects** More: vignette style, midpoint, roundness, feather, highlights; grain size and
  roughness.
- **Optics** (new group): Remove chromatic aberration, Defringe purple and green; More opens the
  two hue ranges. Lens profiles are left out until their ADR.
- **Calibration** (new group, closed by default): shadows tint, then hue and saturation of each
  primary.
- **Parts**: Black & white, Optics and Calibration become parts of a preset, a paste and Batch,
  so a colour preset applied after a black-and-white one turns it back to colour.

## Blocking (fixed in this pass)

- **Coloured tracks lost an end.** Measured against the panels, the dark end of the new B&W mix
  track was 1.0:1 on the dark panel; the green and yellow ends of the calibration tracks 1.7 to
  2.1:1 on the light panel; the existing Temperature, Tint and Saturation tracks fail the same
  way at one end. Every coloured track now has a 1 px rim in `--faint` (5.1:1 dark, 5.2:1
  light), the same fix as the grading wheel. This changes the approved editor's tracks too; the
  app draws it once, in the slider's style.
- **Defaults looked changed.** The first draft showed Blending 50, Detail 25, Feather 50 and
  Roughness 50 in the accent. A value is in the accent only when it differs from its default,
  which is not always 0.

## Improvements

- **Medium.** The range chooser is a radio group: arrow keys move between ranges, Tab goes on to
  the large wheel, which keeps the approved wheel's keys (Left and Right the hue, Up and Down the
  amount, Backspace resets). Each small wheel's name says its values ("Midtones: hue 40
  degrees, amount 12").
- **Medium.** The split marks under the Regions curve are 12 px wide; with their 24 px hit area
  they meet the target size. Keyboard: Tab reaches each mark, Left and Right move it.
- **Low.** Sub-headings inside a group (Sharpening, Vignette, Red primary) are 11 px semibold in
  `--muted`: 7.0:1 dark, 6.8:1 light, at the 11 px minimum. They are labels, not controls.
- **Low.** Greyed Vibrance and Saturation are disabled controls, exempt from contrast; their
  tooltip says why ("No colour in black and white: kept for when you switch back").

## Remove one thing

Removed: a hint under the B&W mix ("How light each colour turns in black and white") repeated
what the group's name and the dark-to-light track already say; the slider's tooltip says it
now.

## Questions for the owner

- None. The layout follows the decisions of 2026-11-05 (More disclosures, midtones and global,
  lens profiles later) and Lightroom's names for every setting.

## What works

- The panel opens as short as today: everything new is either a new closed group or behind
  More, and More remembers its state per group like the groups do.
- Names match Lightroom's, so a preset's settings can be found where its author meant them.
- The library has every operation already (commit "every global Lightroom develop setting"), so
  nothing here is drawn ahead of the engine.
