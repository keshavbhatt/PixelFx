# Changelog

All notable changes to libpixelfx. The format follows Keep a Changelog; the version follows
Semantic Versioning (the API may change before 1.0).

## [Unreleased]

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
