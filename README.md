# PixelFx

A quick photo editor for Linux, with Windows and macOS to follow: make a photo better with
sliders and one-click looks while you watch it change, save your own looks and presets, and
apply them to hundreds of photos at once. Nothing ever overwrites the original.

**Status: being rewritten from scratch.** PixelFx 2 is in development in this repository. The
app is not usable yet; the image library it is built on is (`libs/pixelfx-core`). PixelFx 1
is on the [`legacy-v1`](https://github.com/keshavbhatt/PixelFx/tree/legacy-v1) branch.

## What is here

| Path | What it is |
|---|---|
| `libs/pixelfx-core/` | libpixelfx: the image library and the `pfx` command line tool (Apache-2.0) |
| `DOCS/` | Plan, feature list, design decisions, file formats, coding standards, progress |
| `DOCS/mocks/` | The screens as HTML mocks: open `DOCS/mocks/index.html` in a browser |

## Build the library

```sh
make test   # needs CMake, a C++20 compiler, libvips 8.15+ and LittleCMS 2
build/lib/pfx apply --preset libs/pixelfx-core/examples/moody.pfxpreset -o out/ photo.jpg
```

## Licence

The program is GPL-3.0-or-later, the library is Apache-2.0, and the licensing module has its own
licence: see `LICENSING.md`.
