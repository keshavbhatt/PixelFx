# Design review: editor-raw.html (Camera RAW)

Reviewed 2026-11-06 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: a RAW file opens like any photo and looks as it did on the camera or
phone, then gives the edit the whole range the sensor kept. Signature: the JPEG inside the file
shows at once, and the developed picture takes its place.

## Where it comes from

- Lightroom and Camera Raw: a RAW starts on the maker's look ("Adobe Color" or the camera
  matching profiles), white balance reads in Kelvin with the presets for RAW files only, and a
  JPEG keeps a relative scale. Phones write DNG 1.6 and 1.7 with a gain table map (Samsung
  Expert RAW, Apple ProRAW) that Lightroom applies by default.
- The owner's Galaxy S22 Ultra DNG (2026-11-05): DNG 1.7, linear, JPEG XL compressed, with a full
  size JPEG preview, BaselineExposure +3 and a ProfileGainTableMap. LibRaw cannot read it without
  Adobe's DNG SDK; PixelFx decodes it itself with libjxl.

## Measurements

- The header line is the existing one; "RAW (DNG)" uses the maker's format name for other RAWs.
- "Developing RAW" is the approved busy chip of `editor-zoom-loading.html`: 0.3 s before it
  shows, the same ring and words.
- Temperature in Kelvin: 2000 to 50000 K on the existing temperature track; the value reads
  "5150 K" in `--accent` when changed, as every value does. Tint -150 to +150.
- The Profile and White balance menus are the app's menus; the descriptions under Profile's
  rows are 11 px `--muted` (7.0:1 dark, 6.8:1 light).
- The cannot-read state: the alert in `--danger`, the reason in `--text` 13 px, the preview size
  in `--muted`, the consequence in `--faint` (5.1:1, 5.2:1).

## Accessibility

- The chip is announced once ("Developing RAW"); the swap from preview to developed picture
  needs nothing else.
- "Open its preview instead" is a button with that name; its consequence is said under it.

## Remove one thing

Removed: a "RAW" badge on the filmstrip thumbnail. The header says it for the open photo, and a
badge on every tile of a RAW folder says nothing.

## Questions for the owner

- Kelvin for RAW white balance: approved with the mock (owner, 2026-11-06).
