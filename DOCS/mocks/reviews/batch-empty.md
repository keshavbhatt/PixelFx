# Design review: batch-empty.html (Batch, empty)

Reviewed 2026-11-06 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: an empty Batch page says what goes there and offers the two ways to put
it there. Signature: the welcome page's drop zone, the same shape and words, in the list's place.

## Where it comes from

- The Batch audit (2026-11-06) found the empty list a single muted sentence in its top left
  corner, repeating the head's hint, with Clear and the list or grid switch live and nothing to
  act on. No mock drew this state.
- `welcome.html` (approved): the dashed zone, the images glyph, a heading, a line, the primary
  action first.

## Measurements

- The zone is at most 520 px wide, centred in the list area, a 2 px dashed `--border` on
  `--panel`, 36 px above and below its content; at 1024 by 640 it is 470 px wide and fits with
  60 px to spare above and below.
- Heading 18 px semibold `--text` (14.1:1 dark, 16.0:1 light); the line 13 px `--muted`
  (7.0:1, 6.8:1); the recipe hint 12 px `--faint` (5.1:1, 5.2:1).
- Add photos is the primary button, Add folder secondary, both 32 px tall, 10 px apart: the
  header's two buttons, the same words and glyphs.
- Greyed out: the list or grid switch, Clear and Process (disabled controls are exempt from
  the contrast minimum; each also says why in its tooltip).

## Accessibility

- The zone's buttons are reachable with Tab before the right panel; the zone itself is not a
  control. Drops are announced by the list's own change ("6 photos").

## Remove one thing

Removed: the format list the welcome page has under its zone. Batch reads the same formats, and
the line already says what to drop; the recipe hint is the one thing this page adds.

## Questions for the owner

- Approve the empty Batch page as drawn, with Compress only's words (2) and the accent while
  photos are dragged over it (3).
