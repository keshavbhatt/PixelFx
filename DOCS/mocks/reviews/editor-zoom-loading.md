# Design review: editor-zoom-loading.html (loading the sharp picture)

Reviewed 2026-11-05 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good after one fix. Thesis: when the photo looks soft after zooming in, say that the
sharp picture is on its way, so nobody thinks the edit lowered the quality. Signature: a chip
beside the zoom bar, there only while the wait is long enough to see.

## When it shows

- Zooming in past the preview the first time reads the photo at full size and renders it: under
  a second for a large JPEG, a few seconds for a 24 to 50 megapixel photo or a RAW. The chip says
  "Loading".
- An edit while zoomed in redraws the sharp picture 0.25 s after the slider stops, plus the
  render. The chip says "Updating".
- Only once the wait passes 0.3 s, so a quick render shows nothing and nothing flickers; it goes
  the moment the sharp picture is on show. The full-screen viewer's zoom bar shows the same.
- A ring turning, not a bar: neither the full-size read nor the render reports how far it is.

## Blocking (fixed in this pass)

- **The bar moved.** The first draft put the ring and word inside the zoom bar, which is centred:
  it grew by 90 px, and every button slid 45 px left under the pointer, after each edit while
  zoomed in. The chip now sits outside the bar, 8 px to its right, so the bar never moves.

## Measurements

- The word: 12 px in `--muted` on `--elevated`, 6.2:1 dark and 7.5:1 light.
- The ring: 14 px, a 2 px arc in `--text` (12.5:1 dark, 17.4:1 light) on a track in `--border`;
  the arc carries the shape, the word the meaning.
- The chip is the bar's height (30 px), the same card: `--elevated`, a 1 px `--border`, the
  shadow.

## Accessibility

- The chip is a status: screen readers hear "Loading full size" or "Updating" once, politely.
- Reduced motion: the ring stands still, three quarters drawn; nothing else depends on motion.
- It never takes focus and is never a target.

## Remove one thing

Removed: a separator before the ring, part of the first draft inside the bar.

## Slow preview (2026-11-05)

The owner asked what happens when a preset's or a look's preview takes long to render. Until
now the Preview tag and the line at the top came at once, over the edit still on show: for a
second or more on a large photo, the photo claimed to be a preview it was not yet.

- **The photo is marked when the preview is on it.** The tag and the line come with the
  previewed picture, never before; until then the photo is the edit and says nothing else.
- **The wait shows as the other waits do.** Past 0.3 s the same chip says "Preparing preview",
  zoomed in or not, and goes when the picture arrives. Zoomed in, the screen-size preview comes
  first (the tag with it) and the chip then says "Updating" while the sharp one renders.
- **Only the last tile counts.** Moving on to another tile drops the render of the one left, so
  the photo never shows a preset the pointer is no longer on, and the chip stays up until the
  last one arrives.
- Ending the preview (leaving the tiles, Escape, a click) ends the chip with it.
- Screen readers hear "Preparing preview" once, politely, then the line's text.

## Questions for the owner

- None.
