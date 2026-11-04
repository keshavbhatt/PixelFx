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
