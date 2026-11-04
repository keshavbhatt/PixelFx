# PixelFx

A quick photo editor for Linux, with Windows and macOS to follow: make a photo better with
sliders and one-click looks while you watch it change, save your own looks and presets, and
apply them to hundreds of photos at once. Nothing ever overwrites the original.

**Status: being rewritten from scratch, not released yet.** PixelFx 2 is in development in this
repository. Development builds already open JPEG, PNG, TIFF, WebP, HEIC, AVIF, JPEG XL and
camera RAW photos; edit light, colour, curves, the colour mixer, detail, dehaze and effects
live; crop, straighten and fix perspective; save presets (and bring Lightroom presets along);
and export one photo, a selection, or a whole folder with Batch. PixelFx 1 is on the
[`legacy-v1`](https://github.com/keshavbhatt/PixelFx/tree/legacy-v1) branch.

## What is here

| Path | What it is |
|---|---|
| `app/` | The PixelFx app (Qt 6 Widgets) |
| `libs/pixelfx-core/` | libpixelfx: the image library and the `pfx` command line tool (Apache-2.0) |
| `DOCS/` | Plan, feature list, design decisions, file formats, coding standards, progress |
| `DOCS/mocks/` | The screens as HTML mocks: open `DOCS/mocks/index.html` in a browser |

## Build

On Linux, against the KDE snap SDK the released snap uses (needs the `kde-qt6-core24-sdk`,
`kf6-core24`, `core24` and `cmake` snaps, and `uv` for meson):

```sh
scripts/dev-build.sh --tests
scripts/dev-run.sh -- photo.jpg
```

Anywhere with Qt 6.11, libvips 8.15+ and LittleCMS 2 (as CI does on Linux, Windows and macOS):

```sh
cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build
```

The library alone: `make test`, then
`build/lib/pfx apply --preset libs/pixelfx-core/examples/moody.pfxpreset -o out/ photo.jpg`.

## Licence

The program is GPL-3.0-or-later, the library is Apache-2.0, and the licensing module has its own
licence: see `LICENSING.md`.
