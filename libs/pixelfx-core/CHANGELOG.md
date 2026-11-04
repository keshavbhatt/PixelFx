# Changelog

All notable changes to libpixelfx. The format follows Keep a Changelog; the version follows
Semantic Versioning (the API may change before 1.0).

## [Unreleased]

### Changed

- About three times faster on a 24 MP photo (3.4 s to 1.1 s for load, a typical edit and a
  JPEG save on 14 threads), with a third less memory: pointwise steps read precomputed tables,
  blurs walk memory along rows and run on every core, colour conversion between matrix-shaper
  profiles is a matrix and tabulated curves (LittleCMS for the rest), loading decodes straight
  into the image and saving converts row by row into the output buffer.
- `render` takes the image by move when the caller is done with it.
- `load` refuses images over 512 megapixels with a reason, and reports running out of memory
  as an error instead of throwing.

### Fixed

- Found by fuzzing: a curve whose points meet once clamped into 0 to 1 is now dropped, not
  kept broken; setting names that are empty or contain a dot, and a value that is also the name
  of a group, are dropped with a reason. Every document PixelFx writes reads back the same.

### Added (tooling)

- libFuzzer targets for the document parser and the image loader (`make fuzz`).

### Added

- Geometry: quarter turns, flips, straightening with an automatic inscribed crop, and crop by
  fractions of the image; `pfx::outputSize` gives the rendered size without rendering.
- Effects: vignette (amount, midpoint, roundness), grain (amount, size; repeatable, tied to the
  image so a preview shows the same grain) and fade.
- Colour mixer: hue, saturation and luminance for eight colour bands, blended between
  neighbouring bands and fading out toward grey.
- Colour grading in the split-toning form: a shadow colour, a highlight colour, their amounts
  and a balance; lightness is kept and pure black and white stay neutral.
- Perspective: vertical and horizontal keystone correction (`geometry.perspective.*`), with no
  empty corners and no change of size.
- Dehaze (dark channel prior with a guided filter, worked out on a copy about 1000 pixels
  across so a preview and the export share the same haze map); negative values add haze.

## [0.1.0] - 2026-11-04

### Added

- Float RGBA image in linear Rec.2020, colour maths (sRGB and perceptual encodings, Bradford
  adaptation, Planckian white points).
- Settings with dotted keys, the operation registry with parameter ranges and units, and
  PixelFx documents (presets and recipes) in JSON with unknown keys preserved.
- Operations: exposure, tone, white balance, vibrance and saturation, curves, clarity,
  sharpening, noise reduction.
- Loading and saving through libvips with colour management through LittleCMS.
- The `pfx` command line tool.
