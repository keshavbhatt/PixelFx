# Design review: export-sizes.html (Export, size presets)

Reviewed 2026-11-04 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: choose where the photo is going, not how many pixels it needs.
Signature: when the destination's shape differs from the photo, a framing preview shows what is
kept and lets the user drag the frame, instead of a silent centre crop.

## Blocking

None. Pixel sizes in the menu are `muted` on `elevated`: 6.2:1 (dark), 7.5:1 (light). The
current choice has a tick and a highlight, not colour alone.

## Improvements

- **Medium.** The list is long (14 rows); grouped headings keep it scannable. If the owner adds
  more destinations, a search field at the top of the menu becomes necessary.
- **Medium.** "Facebook post" and "Web" are both 2048 on the long edge. Kept on purpose: users
  look for the destination's name, and the pixel size beside it shows they are the same.
- **Low.** The framing popover sits beside the menu; on windows narrower than 1280 px it moves
  under the Size row.
- **Removed one thing.** "Save current as a size preset" duplicated "Custom size", which now
  offers to keep the size under "Mine".

## What works

- Destinations people recognise, with the exact pixel size and ratio on each row.
- Crop to fill or Fit inside is explicit, and the export frame never changes the photo's own
  crop.
- The same list feeds Batch and Crop's ratio chips, so a user learns it once.
