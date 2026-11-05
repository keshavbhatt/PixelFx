# Design review: batch-convert-rename.html (convert, resize and rename in Batch)

Reviewed 2026-11-06 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: the everyday file jobs (HEIC to JPG, smaller for email, a set of names)
take one click to start and show their result before they run. Signature: the first three file
names, as they will be saved, under the name field while it is typed.

## Where it comes from

- FEATURES M8: "Convert and resize only... one click from the welcome page". Batch could already
  do it ("None: resize and save only"), but only for someone who knew to open Batch and look in
  the recipe's menu. The welcome page now offers it, with Compress only next to it.
- FEATURES M9: batch rename with tokens and a live preview. Lightroom's Filename Template
  Editor and Adobe Bridge's Batch Rename show a preview of one name; this shows three, so a
  counter's padding and a token's value read at a glance.
- FEATURES M12: the number of photos worked on at once. The approved `settings.html` calls it
  "Batch threads"; each photo already uses every core, so what the setting really chooses is
  photos at a time, and the mock says that.

## Measurements

- Quick jobs: the `btn sm` buttons of the app (28 px), under the three main buttons so the main
  choice stays first; "Quick jobs" in `--muted`, 7.0:1 dark and 6.8:1 light.
- The name preview: 12 px monospace; the source name in `--muted` (7.0:1, 6.8:1), the new name
  in `--text` (14.1:1, 16.0:1), the arrow in `--faint` (5.1:1, 5.2:1). Three rows and "and N
  more" add 78 px under the field; the Save as section still fits a 900 px window without the
  panel scrolling more than before.
- The clash line: `--warning` with the alert icon, 9.0:1 dark and 4.6:1 light, under the preview
  it explains.
- The token menu: 360 px, a row per token with its example from the first photo in `--faint`.

## Behaviour

- Convert or resize opens Batch in Edit and export with the recipe "Convert and resize only"
  (the renamed first choice); Make files smaller opens it in Compress only. Both then wait for
  photos, as Batch edit does.
- + Token inserts at the cursor. Tokens: {name}, {n}, {taken}, {date}, {camera}, {w}x{h}
  ({w} and {h} also alone), {preset}. Anything else typed is kept as text; prefix and suffix
  need no field of their own.
- {n} counts in the list's order from 1, padded to the width of the count. {taken} reads the
  Exif date; a photo without one gets today.
- The preview updates as the field changes, from the first three photos of the list; it also
  shows in Compress only, whose names keep each file's own format.

## Accessibility

- The preview is text, read in order: "IMG_0412.HEIC becomes trip-001.jpg" (the arrow is
  given that name for screen readers).
- + Token is a menu button with a name, reachable by Tab after the field; the menu lists the
  token and what it means, never the token alone.

## Remove one thing

Removed: a separate Prefix and Suffix field pair from the first draft. Text around {name} does
the same, and one field with a preview is easier to read than three.

## Questions for the owner

- None.
