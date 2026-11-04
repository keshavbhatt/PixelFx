# libpixelfx

The image library behind PixelFx: non-destructive photo adjustments in C++20, with no Qt and
no other application framework. An edit is a set of settings (a recipe); the library renders
it onto an image in a fixed order, in float linear light, and the same settings give the same
look on a small preview and on the full-size photo.

Apache-2.0. Part of the PixelFx repository (`libs/pixelfx-core`), built and tested on its own.

## What it does (0.1)

- Loads JPEG, PNG, TIFF, WebP and every other format libvips reads; applies the orientation
  tag; converts through the embedded colour profile into linear Rec.2020.
- Operations: exposure, tone (contrast, highlights, shadows, whites, blacks), white balance
  (temperature, tint), vibrance and saturation, curves (RGB and per channel), clarity,
  sharpening, noise reduction (luminance and colour); geometry (quarter turns, flips,
  straightening, crop); colour mixer (eight bands); effects (vignette, grain, fade).
- Reads and writes PixelFx preset and recipe files (`DOCS/presets.md` in the repository).
- Saves JPEG, PNG and TIFF (8 or 16 bit), WebP, AVIF and JPEG XL with an embedded sRGB profile,
  keeping Exif, XMP and IPTC metadata.
- `pfx`, a command line tool: `pfx ops`, `pfx info`, `pfx apply`.

## Build

Needs CMake 3.21, a C++20 compiler, libvips 8.15 or later (`vips-cpp`) and LittleCMS 2.14 or
later. nlohmann/json and doctest are fetched at pinned versions when the system has none.

```sh
cmake -S libs/pixelfx-core -B build/lib -G Ninja -DPFX_WERROR=ON
cmake --build build/lib
ctest --test-dir build/lib
```

## Use

```cpp
#include <pfx/io.h>
#include <pfx/pipeline.h>

auto photo = pfx::load("photo.jpg");
pfx::Settings settings;
settings.set("light.exposure", 0.5);
settings.set("color.vibrance", 25.0);
auto result = pfx::render(photo.value().image, settings);
pfx::save(result.value(), "photo-edit.jpg", {}, &photo.value().metadata);
```

```sh
pfx apply --preset examples/moody.pfxpreset --set light.exposure=0.3 -o out/ *.jpg
pfx ops   # every operation and its parameters, ranges and units
```

Every fallible call returns `pfx::Result` or `pfx::Status`; no exceptions cross the API. The
library logs nothing unless the host sets `pfx::setLogSink`.

## Tests

`tests/` uses doctest. Test images are generated in code (`tests/fixtures.cpp`), so the
repository holds no third-party pictures. `tests/golden/` holds each operation's expected
output as a 16-bit PNG; regenerate them only on purpose with `PFX_GOLDEN_UPDATE=1` and review
the images like any other change. Scale tests check that a half-size render matches the
full-size render downscaled.
