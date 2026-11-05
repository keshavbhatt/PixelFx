# Design review: editor-mixer-target.html (a colour picked from the photo)

Reviewed 2026-11-05 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: change "that sky" or "her skin" where it is, instead of guessing which of
eight swatches holds it. Signature: drag up or down on the photo itself, and the mixer follows,
naming the bands that move.

## Where it comes from

Lightroom's Targeted Adjustment Tool in its HSL and B&W panels: a click on the photo picks the
colour, a vertical drag changes it, and a colour between two bands moves both by their share.
The mock keeps all of that; the button sits where PixelFx already puts a picker (White balance),
and the shortcut is one key, T, instead of Lightroom's chord.

## Behaviour

- T or the target button turns it on; Escape, T, the button, or leaving Edit turns it off. While
  it is on, the pointer is a target over the photo and the line at the top says what a drag does
  in the mode on show ("Drag on a colour to change its luminance").
- A click selects the swatch of the colour under the pointer. Dragging changes the mode on show
  (Hue, Saturation or Luminance; in black and white the B&W mix): up raises, down lowers, one step
  per 2 px, Shift for finer steps. A colour between two bands moves both by its share of each, as
  the engine blends bands; the panel shows the larger and says "Also moves Aquas luminance -10".
- The colour is read from the photo before the mixer, so it stays put while the drag changes it.
- One drag is one history step, named by its bands ("Blues and aquas luminance -24").
- Hovering before a click shows the hint with the bands under the pointer, so the user sees what
  will move before moving it.

## Measurements

- The target button is the 26 px icon button of the White balance picker. Off: its icon in
  `--text`, 14.1:1 dark and 16.0:1 light. On: the accent on `--accent-soft`, 6.0:1 and 4.9:1,
  with `aria-pressed`.
- The marker is an 18 px ring, white with dark inner and outer edges: white alone is 1.8:1 on a
  light sky, the dark edge 6.9:1 there, so one edge always shows (the approach of the heal brush).
- The hint: 12 px on `--elevated`, the bands in `--text` (12.5:1 dark, 17.4:1 light), the drag
  line in `--muted` (6.2:1 and 7.5:1). It sits 18 px right of the marker and moves to the left
  near the photo's right edge, so it never covers the colour being changed.

## Accessibility and platform

- The tool is a faster way, never the only one: every band stays a slider with its keys.
- The button's name is "Pick a colour on the photo"; its tooltip gives T. T joins the shortcuts
  sheet, and "Adjust a colour on the photo" joins the Photo menu, so it is reachable from the menu
  as every command is (DESIGN lens 2).

## Remove one thing

Removed: a target icon before "Also moves Aquas luminance -10". The line explains the slider
above it; the icon only repeated the button's.

## Questions for the owner

- None. T is free in Edit (V, W, J, H, E, C, R and B are taken); Color grading gets no picker,
  as in Lightroom.

## What works

- Nothing new to learn in the panel: the same swatches and sliders, one more button.
- In black and white it answers the question the B&W mix cannot: which colour a grey came from.
