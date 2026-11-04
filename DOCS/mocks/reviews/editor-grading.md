# Design review: editor-grading.html (Color grading)

Reviewed 2026-11-05 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good after one fix. Thesis: give a photo a colour mood (cool shadows, warm light) in
two gestures. Signature: two wheels where the angle is the colour and the distance is the
strength, with the numbers written underneath, so the gesture and the value agree.

## Blocking (fixed in this pass)

- **The wheel's edge disappeared in the light theme.** The yellow part of the hue ring measured
  1.6:1 against the light panel (#E0C04F on #F5F5F5), under the 3:1 for controls. The wheel
  now has a 1 px rim in `--faint`: 5.1:1 dark, 5.2:1 light.

## Improvements

- **Medium.** The puck is white with a dark outer ring: white alone is 1.8:1 on the yellows
  but the ring is 11.8:1 there, and white is 3.8:1 on blue and 4.3:1 on the grey centre, so one
  of its two edges always passes 3:1.
- **Medium.** Pointer-only gestures need a keyboard twin: each wheel takes focus, Left and Right
  turn the hue (1 degree, Shift 10), Up and Down change the amount, Backspace or double-click
  resets; screen readers hear "Shadows colour: hue 200 degrees, amount 20".
- **Low.** Text under the wheels is 12 px: muted 7.0:1 dark and 6.8:1 light, the changed
  amount in the accent 6.9:1 dark and 5.3:1 light. The 104 px wheel is far above the 24 px
  target minimum; the 14 px puck is not a separate target.
- **Nothing by colour alone.** Hue and amount are always written out.

## Questions for the owner

- **Midtones.** Lightroom-style grading has a third wheel for the midtones. The file format
  (`DOCS/presets.md`, `grading`) has shadows, highlights and balance only; a midtones wheel
  would change the format (a version bump and a migration). Two wheels, or three?
- **Where it lives.** FEATURES E8 says "a Color group sub-panel"; the mock makes it its own
  group after Color mixer, like the mixer, which keeps the Color group short. Own group, or
  inside Color?

## What works

- Same group pattern as the rest of the panel: dot and reset when changed, collapsible.
- Balance is an ordinary slider with the same changed styling.
- The library operation already exists (`grading`, stage 90): lightness is kept and pure black
  and white stay neutral, so the wheels cannot clip a photo.
