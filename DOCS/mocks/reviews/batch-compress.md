# Design review: batch-compress.html (Batch, compress only)

Reviewed 2026-11-04 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: make a pile of files smaller and see exactly what it saved. Signature:
every row shows its size before and after with a bar, and any row can be checked at 100% next
to its original, so a lossy setting is judged by eye, not trusted blindly.

## Blocking

None. Measured: the "Saved" pills use `success` on `hover`-grey pills (dark 5.6:1, light
4.6:1); the size figures are `text` on `panel`; the bars are `accentStrong` on `track`
(non-text, 3:1 or better in both themes). Saving is stated in words and numbers, not only by
bar length.

## Improvements

- **Medium.** "No change" must say why on the row ("Already as small as it gets"), which the
  mock does; a row that failed gets its reason in the same place (`batch-done.html` rule).
- **Medium.** "Replace the originals" is irreversible: it asks once per batch, with the number
  of files, and is never the default.
- **Low.** The compare strip takes vertical space even before anything is selected; it appears
  only after the first file is done.
- **Removed one thing.** "Also convert to WebP, AVIF or JPEG XL" mixed a second job into this
  mode; converting is the "Convert and resize only" batch (FEATURES M8), so the option went.

## What works

- Compress only is a mode of the same Batch page, not another app inside the app.
- Defaults are safe: save into a folder, keep the original when the result is not smaller,
  JPEG lossless.
- The "Free for any number of photos" line sits where the user chooses the mode. That is
  product text, not licensing text, but it is checked against the release-text rule before
  shipping.

## Changed while building (2026-11-06)

- **Others.** "WebP, AVIF, GIF and TIFF re-encoded only when smaller" promised what cannot be
  kept: there is no AVIF encoder in the build, and encoding a lossy WebP again loses quality. The
  line now says "WebP, GIF and TIFF made smaller without loss when they can be"; a file left as
  it is gets its reason on its row ("A lossy WebP stays as it is", "HEIC is kept as it is").
- **Location and previews.** The check box's text no longer carries its own parenthesis; "Camera
  details and copyright stay" is a muted line under it, so the box's name stays short.
- **Rows.** The ⋯ button became the × that leaves a file out, as on the Edit and export list:
  there was nothing else for its menu to hold.
- **The mode switch** ("1. What to do") is at the top of Edit and export too, whose sections are
  now 2. Recipe to 5. Save as; the mode is remembered.
- **JPEG Target quality** shows a JPEG quality slider under it, as Fewer colours shows PNG
  quality. A JPEG is encoded again only when that is smaller than its lossless result.
- **Run again with these settings** replaces the files the last run of the same list wrote,
  instead of numbering them.
- **The before and after** shows the most detailed spot of the photo, found on a small copy;
  when the originals are replaced, that spot of the original is kept during the run so the
  before still shows.
- Lossless JPEG drops MPF previews always: they live after the image and cannot survive a
  transcode. Text and numbers measured as in the mock: the same tokens and pills.

