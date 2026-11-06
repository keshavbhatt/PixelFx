# Design review: the app icon (`app/data/icons/hicolor/`)

Reviewed 2026-11-06 with the method in `DESIGN.md` section 5. The owner supplied the icon (a set
made with the same Flathub checks), after a round of concepts here; it replaces PixelFx 1's
raster colour flower and keeps the colour-wheel spirit of PLAN D17.

## Summary

Rating: Good. Thesis: a camera and colour in one mark. Signature: a lens ring around six
spectrum blades, red to purple, closing on a dark opening.

## Checks (Flathub's app-icon guidelines)

- **Shape.** The GNOME circle base (r 52) with a 4 px side; 81 by 84 % of the canvas, 8 px clear
  of every edge. No filters, text or bitmaps. 0 fail, 0 warn.
- **Contrast.** The light ring keeps the edge visible on a dark window; the grey side and bezel
  draw it on a light one.
- **Small sizes.** At 16 px the blades read as a colour disc in a ring.
- **Qt.** Qt's SVG renderer ignores `<clipPath>` (KDE draws desktop icons with it), so the
  blades spilled past the lens as a hexagon. Each blade is now the exact part of it inside the
  lens (lines and an arc), the clip path gone: the same picture in Cairo within anti-aliasing
  (no pixel differs by more than 16 of 255 at 512 px), right in Qt. The symbolic icon the same.
- **Brand colours.** #99c1f1 and #2a5a9e, from the owner's set.

## Improvements

- **Low.** A colourful aperture is a common photo motif; this one's ring and six even blades are
  its own, but reviewers compare with other photo apps' marks.

## In the app

- The header's mark (every mock's `.brand .mark`, approved by the owner 2026-11-06), About and
  the window all show it; About draws it as the mock does, on a 72 px canvas.
