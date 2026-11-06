# Design review: the app icon (`app/data/icons/hicolor/`)

Reviewed 2026-11-06 with the method in `DESIGN.md` section 5, before the owner's review. The
owner asked for a new icon, then for a totally new one rather than the colour-wheel spirit of
PLAN D17; of six concepts they chose the before and after split.

## Summary

Rating: Good. Thesis: the same photo, better. Signature: one photo cut by a white diagonal,
grey and flat before it, a vivid sunset after it; the line is the app's before and after view.

## Checks (Flathub's app-icon guidelines)

- **Shape.** The GNOME square base, 96 by 88 px with 12 px corners on the 128 px canvas, a 4 px
  side for thickness; 75 by 72 % of the canvas, 16 px clear of every edge. No filters, text or
  bitmaps.
- **Contrast.** No dark outer edge (the checker finds 0 %). The grey half's sky against a light
  window (#c0bfbc on #fafafa) 1.8:1, its edge still drawn by the 4 px side; on a dark window
  (#242424) 8.4:1. The purple hill on the sky 2.2 to 3.6:1, the sun on the sky behind it about
  2.4:1 (a darker sky at the top: 1.4:1 with the first draft's light top, fixed).
- **Small sizes.** At 16 and 24 px the grey and orange halves and the white line still read;
  the sun is the first detail to go, at 16 px.
- **Symbolic.** A 16 px frame (2 px), the diagonal (2 px), a sun and a hill in one colour.
- **Brand colours.** Mint #8de0d8 and deep teal #1f6f74, away from the icon's orange and purple
  (1.7:1 and 3.6:1 against the nearest icon colour) so the icon stands out on both banners.

## Improvements

- **Medium.** The header's 22 px colour wheel (every mock's `.brand .mark`, and `BrandMark` in
  the app) is the old identity. The mocks now show the icon there and in About; the header in
  the app waits for the owner's approval.
- **Low.** The name PixelFx is in two capitals, which Flathub's name check flags; it is the
  name PixelFx 1 shipped under (PLAN D4), so it stays.

## What works

- It says what the app does, not only that it handles photos: a generic picture icon with a
  reason to be this one.
- No other app's mark: no petals, pinwheel or aperture.
