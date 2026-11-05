# Presets, looks, recipes and batch recipes: the file formats

Status: DRAFT for owner review (M0). Becomes binding with ADR-005 once approved; from then on
any change bumps `version` and adds a migration where documents are read (`readEnvelope` in
`libs/pixelfx-core/src/recipe/document.cpp`), with a test.

One JSON envelope for every kind, read and written by libpixelfx (`pfx::Document`), so the
app, the `pfx` CLI and any other program using the library share one implementation.

| Kind | Extension | What it is | Who makes it |
|---|---|---|---|
| `recipe` | `.pfx` (sidecar) | Every setting of one photo, with history and snapshots | The editor, automatically |
| `preset` | `.pfxpreset` | Part of a recipe (the ticked groups), optionally with a look | Save as preset, XMP import |
| `look` | `.pfxlook` | An ordered stack of steps with one strength (the user's "filter") | Look editor, LUT import, built in |
| `batch` | `.pfxbatch` | A stack of presets and looks plus resize and output options | Batch page, Save as batch recipe |

## 1. Envelope

```json
{
  "format": "pixelfx",
  "kind": "preset",
  "version": 1,
  "id": "7b0f2c7e-3c1a-4d0e-9a51-2f5a2b0c9e11",
  "name": "Moody landscape",
  "group": "My presets",
  "created": "2026-11-04T18:20:00Z",
  "modified": "2026-11-04T18:20:00Z",
  "generator": "PixelFx 2.0.0 (libpixelfx 1.0.0)",
  "workingSpace": "rec2020-linear",
  "body": { }
}
```

- `format` and `kind` are checked first; an unknown `kind` or a `version` newer than the
  library's is refused with a plain error ("made by a newer PixelFx").
- `id` is a UUID that survives renames, so a recipe can refer to a look by id.
- Unknown keys are kept on load and written back on save (forward compatibility within a
  version).
- Text is UTF-8, numbers are JSON numbers, no comments. Files above 32 MB are refused.

## 2. Settings (shared by recipe, preset and batch)

Settings are grouped exactly as the right panel groups them. A preset contains only the groups
the user ticked; a missing group means "leave the photo's own value". Values are in the units
the operation registry declares for the UI (the same numbers the sliders show), never in
internal units, so a file reads like the panel.

```json
"settings": {
  "light":   { "exposure": 0.35, "contrast": 18, "highlights": -35, "shadows": 28, "whites": 6, "blacks": -10 },
  "color":   { "wb": { "mode": "relative", "temperature": 14, "tint": 0 }, "profile": "standard", "vibrance": 22, "saturation": 0 },
  "mixer":   { "hue": { "yellow": -8 }, "saturation": { "green": -30 }, "luminance": {} },
  "curves":  { "rgb": [[0,0],[0.28,0.2],[0.72,0.76],[1,1]], "r": null, "g": null, "b": null,
               "parametric": { "shadows": 0, "darks": 0, "lights": 0, "highlights": 0,
                               "split": { "shadows": 25, "midtones": 50, "highlights": 75 } } },
  "grading": { "shadows": { "hue": 200, "amount": 20, "luminance": 0 }, "midtones": { "hue": 40, "amount": 8, "luminance": 0 },
               "highlights": { "hue": 35, "amount": 15, "luminance": 0 }, "global": { "hue": 0, "amount": 0, "luminance": 0 },
               "balance": 10, "blending": 50 },
  "bw":      { "enabled": false, "mix": { "red": 0, "orange": 10, "yellow": 0, "green": 0, "aqua": 0, "blue": -30, "purple": 0, "magenta": 0 } },
  "calibration": { "shadows": { "tint": 0 }, "red": { "hue": 0, "saturation": 0 }, "green": { "hue": 0, "saturation": 0 },
                   "blue": { "hue": 0, "saturation": 0 } },
  "detail":  { "clarity": 15, "texture": 0, "sharpen": { "amount": 30, "radius": 0.0004, "masking": 0, "detail": 25 },
               "denoise": { "luminance": 0, "luminanceDetail": 50, "luminanceContrast": 0, "color": 25, "colorDetail": 50,
                            "colorSmoothness": 50 }, "dehaze": 0, "ca": { "auto": false },
               "defringe": { "purple": { "amount": 0, "hueLow": 30, "hueHigh": 70 }, "green": { "amount": 0, "hueLow": 40, "hueHigh": 60 } } },
  "effects": { "vignette": { "amount": -22, "midpoint": 50, "roundness": 0, "feather": 50, "style": "highlight", "highlights": 0 },
               "grain": { "amount": 0, "size": 25, "roughness": 50 }, "fade": 0 },
  "geometry":{ "crop": { "x": 0.04, "y": 0.03, "w": 0.88, "h": 0.87 }, "angle": 1.5, "rotate90": 0, "flipH": false, "flipV": false,
               "perspective": { "vertical": 0, "horizontal": 0, "corners": null } },
  "look":    { "id": "builtin:golden-hour", "strength": 72 },
  "heal":    { "spots": [ { "x": 0.31, "y": 0.22, "source": { "x": 0.36, "y": 0.24 }, "radius": 0.015,
                            "feather": 50, "opacity": 100 },
                          { "x": 0.6, "y": 0.7, "source": { "x": 0.6, "y": 0.66 }, "radius": 0.01,
                            "feather": 30, "opacity": 100, "stroke": [[0.6, 0.7], [0.64, 0.71], [0.7, 0.7]] } ] }
}
```

- Keys inside a group are optional; a missing key in a present group is the operation's
  default. Ranges come from the registry; out-of-range values are clamped on load and
  reported once.
- Spatial values (`radius`, crop rectangle) are fractions of the long edge or of the image, never
  pixels; grain `size` is a 0 to 100 slider the operation turns into 0.05% to 0.3% of the long
  edge, so the same file gives the same result on a preview and on the
  full-size export (LESSONS row 3).
- White balance in a preset is `relative` (shift from the photo's own) by default; `absolute`
  (Kelvin) is stored only when the user picked Kelvin.
- `heal.spots` (recipes version 2, FEATURES E20) lists the photo's healed spots in the order
  they were made. `x`, `y` and `source` are fractions of the upright photo's width and height
  **before** turns, flips, straightening, perspective and the crop (healing runs first, section
  5), so changing the crop later never moves a spot. `radius` is a fraction of the long edge
  (0.001 to 0.25), `feather` and `opacity` 0 to 100. A `stroke` of two or more points (the
  first at `x`, `y`) is a brush stroke healed along its length with the source moved by the
  same offset. Values outside the ranges are clamped; a list that cannot be read is dropped
  with a warning; at most 1000 spots and 4096 points per stroke are kept. Spots belong to one
  photo: Save as preset, copy and paste, and batch recipes never carry them.
- The Lightroom-compatible settings (presets version 2, recipes and batch recipes version 3,
  FEATURES P8), each defaulting to no change: `bw` turns the photo black and white and sets how
  light each colour band turns (-100 to 100). `grading` has four ranges (shadows, midtones,
  highlights, global), each with a hue, an amount and a luminance (-100 to 100), plus balance and
  blending (0 to 100, 50 the even overlap). `calibration` moves the red, green and blue primaries
  (hue and saturation, -100 to 100) and tints the shadows (-100 green to 100 magenta), keeping
  greys neutral. `color.profile` is the base rendering: `standard`, `vivid`, or `camera` (presets
  version 3, recipes and batch recipes version 4: a RAW file's own tone and the base curve its
  camera's JPEG has; offered for RAW files). `curves.parametric`
  lifts or lowers four tone regions (-100 to 100) split at three points (5 to 95). `detail.texture`
  (-100 to 100), `detail.sharpen.detail` (0 to 100, 25 the plain mask), the noise reduction detail,
  contrast and smoothness (0 to 100), `detail.ca.auto` (lateral chromatic aberration measured on
  the photo) and `detail.defringe` (purple and green, amount 0 to 20 and a hue range 0 to 100).
  `detail.lensProfile` (presets version 4, recipes and batch recipes version 5; ADR-013):
  `enable` corrects the photo's lens from its profile, found from the photo's own camera and
  lens, with `distortion` and `vignetting` amounts (0 to 200, 100 the profile as measured).
  `effects.vignette` gains `feather` (0 to 100), `style` (`highlight`, `color` or `paint`) and
  `highlights` (0 to 100); `effects.grain` gains `roughness` (0 to 100, 50 one even layer).
- `look.id` refers to a look by id: `builtin:<slug>` for shipped looks, a UUID for the user's.
  A file that must stand alone (export, sharing) embeds the look instead:
  `"look": { "embedded": { ...look body with its id and name... }, "strength": 72 }`; a batch
  recipe keeps its embedded looks once each in its `looks` list (section 4).

## 3. Look body (the user's "filter")

```json
"body": {
  "category": "color",
  "defaultStrength": 80,
  "steps": [
    { "op": "curves",    "v": 1, "enabled": true,  "params": { "rgb": [[0,0.04],[0.5,0.48],[1,0.97]] } },
    { "op": "grading",   "v": 1, "enabled": true,  "params": { "shadows": { "hue": 200, "amount": 20 }, "highlights": { "hue": 35, "amount": 15 }, "balance": 10 } },
    { "op": "mixer",     "v": 1, "enabled": true,  "params": { "saturation": { "green": -30 }, "hue": { "blue": -10 } } },
    { "op": "lut",       "v": 1, "enabled": true,  "params": { "resource": "portra", "mix": 60 } },
    { "op": "grain",     "v": 1, "enabled": false, "params": { "grain": { "amount": 18, "size": 2 } } },
    { "op": "vignette",  "v": 1, "enabled": true,  "params": { "vignette": { "amount": -15, "midpoint": 50 } } }
  ],
  "resources": {
    "portra": { "type": "lut3d", "size": 33, "domain": "srgb-gamma", "domainMin": [0,0,0], "domainMax": [1,1,1],
                "interpolation": "tetrahedral", "encoding": "f16-base64", "name": "kodak-portra", "data": "..." }
  }
}
```

- `op` is an operation id of the registry (`pfx ops` lists them), or `lut`. `params` are that
  operation's settings written under its group, as in section 2 without the group's own name
  (`exposure` reads `{"exposure": 0.5}` for `light.exposure`; `vignette` reads
  `{"vignette": {...}}` for `effects.vignette.*`); values are clamped to the registry's ranges
  on load. A `lut` step names a resource and a `mix` from 0 to 100. `geometry` is never a look
  step. `v` is the operation's parameter version (each operation migrates its own params).
- An unknown `op` is kept as it is (a newer PixelFx may know it), reported, and skipped when
  rendering; the look never fails to load as a whole for it.
- Steps run top to bottom on the image as it is after the photo's own settings up to colour
  grading (the look position in section 5). `strength` blends the look's result with its input
  in linear light: 0 is no change, 100 is the full look.
- LUTs are embedded, so a `.pfxlook` is one self-contained file: `type` `lut3d` or `lut1d`,
  `size` (entries per edge, 2 to 256; 1D up to 65536), the domain the `.cube` declared, and the
  table as little-endian IEEE half floats in base64, R, G, B per entry, red fastest. `domain` is
  the encoding the LUT expects as input and is mandatory, because a LUT applied in the wrong
  encoding is the commonest LUT bug; version 1 reads `srgb-gamma` (sRGB primaries and curve),
  and a resource in any other domain, or whose data does not match its size, is left out with a
  warning (the steps using it are skipped). A `sha256` key is reserved for checking and
  deduplicating imports; version 1 ignores it and does not write it.
- Imported `.cube` (1D and 3D, `DOMAIN_MIN` / `DOMAIN_MAX` and `LUT_3D_INPUT_RANGE` honoured)
  and HaldCLUT pictures become `lut1d` / `lut3d` resources.
- Each step may later carry `"mask"` (local adjustments, FEATURES E19); the key is reserved.
- "Bake to .cube" evaluates the colour-only steps (those the registry marks `pointwise`) on a
  33-point grid; spatial steps (grain, vignette, clarity) are listed as left out.
- History: the M0 draft named example steps (`tone_curve`, `split_tone`, `hsl`) that never
  existed in the registry and left the params' shape open. No look file was written before
  libpixelfx read and wrote looks (2026-11-05), so this text is version 1 itself, not a change
  to it.

## 4. Recipe (sidecar) and batch bodies

```json
"body": {
  "source": { "name": "sunset-lake.jpg", "size": 8812341, "fingerprint": "xxh3:9f1c0a5be2d47a10" },
  "settings": { ...all groups... },
  "history": [ { "label": "Shadows +28", "settings": { "light": { "shadows": 28 } } } ],
  "snapshots": [ { "name": "Warmer", "settings": { ... } } ]
}
```

- Recipes are at **version 5**: version 2 added the `heal` group (section 2), version 3 the
  Lightroom-compatible settings (section 2), version 4 the `camera` profile, version 5
  `detail.lensProfile`. Older recipes read unchanged (nothing to migrate: an older file never
  names either); a file above version 5 is refused as made by a newer PixelFx. Presets are at
  **version 4** for the same settings; version 1 to 3 presets read unchanged.
- The fingerprint (size plus a hash of the first and last 64 KiB) re-attaches a sidecar to a
  photo that was moved or renamed within the library folder.
- History entries store only the changed groups (deltas), capped at 200 steps per photo.

```json
"body": {
  "stack": [ { "look": { "id": "builtin:golden-hour", "strength": 72, "name": "Golden hour" } },
             { "look": { "id": "1f0c2b9e-...", "strength": 60, "name": "Dusk walk" } },
             { "preset": { "embedded": { "name": "Product white", "settings": { ... } } } } ],
  "looks": [ { "id": "1f0c2b9e-...", "name": "Dusk walk", ...look body (section 3)... } ],
  "autoFirst": false,
  "keepExistingEdits": false,
  "resize": { "mode": "longEdge", "value": 2048, "noEnlarge": true },
  "output": { "format": "jpeg", "quality": 90, "colorSpace": "srgb", "metadata": { "camera": true, "location": false },
              "folder": "~/Pictures/PixelFx/Batch", "beside": false, "name": "{name}-{preset}", "ifExists": "number",
              "watermark": { ...a watermark document (section 7)..., "logoData": "<base64 PNG or SVG>" } }
}
```

Batch recipes are at **version 5** (version 3 added the Lightroom-compatible settings of
section 2, version 4 the `camera` profile, version 5 `detail.lensProfile`; looks stay at
version 1).

- The stack is applied in order onto each photo's settings (later entries win per key). A step
  is a preset (its settings, embedded) or a look on its own (`id`, `strength` 0 to 100,
  `name` for people). A step is written as a look step when its settings are only `look.*`.
- `looks` (version 2) embeds every look of the user's that the stack refers to, once each, as
  a look body (section 3) with its `id` and `name`, so the file runs on another machine;
  `builtin:` references stay references. Reading also accepts a look embedded in its step,
  `"look": { "embedded": { ... }, "strength": 72 }` in a look step or in a preset's settings
  (section 2); it is kept once in `looks`. An embedded look without an id of its own is left
  out with a warning. Opening the file adds those looks to the user's looks when an id is not
  there yet.
- `resize` (version 2 adds `preset`): `{ "mode": "none" }`, `{ "mode": "longEdge", "value":
  2048, "noEnlarge": true }`, or `{ "mode": "preset", "preset": "instagram-portrait",
  "fitInside": false }`, a size of the Export sheet by id (crop to fill, or fit inside with a
  border); an id this version does not know means the original size.
- `output.watermark` (version 2): the whole watermark document of section 7, and for a logo
  `logoData`, the logo file's bytes in base64, so the mark travels with the recipe. Absent: no
  watermark. Opening the file adds the watermark to the user's watermarks unless an identical
  one is there already.
- Version 1 batch recipes had none of these keys and read unchanged; there is nothing to
  migrate. A file with a `version` above 5 is refused as made by a newer PixelFx.

## 5. Pipeline order (fixed, documented here and in ADR-004)

Input decode and orientation, RAW development, then: lens profile -> chromatic aberration ->
defringe -> spot heal -> geometry (crop, rotate, perspective, as one resampling) -> noise
reduction -> white balance -> calibration -> profile -> exposure -> dehaze -> tone (highlights, shadows, whites,
blacks, contrast) -> clarity -> texture -> curves (parametric, then points) -> colour mixer ->
black and white -> vibrance and saturation -> colour grading -> **look** (blended by strength)
-> sharpen -> vignette -> grain -> fade -> output transform (colour space, bit depth). Identity
settings are skipped.

## 6. Imports

| Source | Becomes | Mapping |
|---|---|---|
| `.cube` (1D, 3D), `.3dl`, HaldCLUT `.png` | look with one LUT step | exact |
| RawTherapee `.pp3` | LATER (subset) | |
| Lightroom / Camera Raw `.xmp` preset (`crs:` namespace) | preset | Every global develop setting (FEATURES P8): Basic (Exposure2012 to Blacks2012, Temperature and Tint relative or Kelvin, Vibrance, Saturation, Texture, Clarity2012, Dehaze), Treatment and ConvertToGrayscale, HSL and GrayMixer (8 bands), ToneCurvePV2012 (and per channel) and the parametric curve with its splits, SplitToning and ColorGrade (all four ranges, luminance, blending, balance), Detail (sharpening amount, radius, detail, masking; noise reduction with detail, contrast and smoothness), AutoLateralCA and Defringe, PostCropVignette (with feather, style, highlights), Grain (with roughness), Calibration (primaries and shadow tint), and the profile (Adobe Monochrome as black and white, Adobe Vivid as the vivid profile, the other Adobe standard profiles as PixelFx's own rendering). The same kind of result, not pixel-identical. LensProfileEnable with its distortion and vignetting scales as the lens profile (ADR-013). A report lists what is skipped: the manual lens corrections, creative and camera profiles PixelFx does not have, masks |
| darktable style (`.dtstyle`) | LATER | |

## 7. Watermark (`.pfxwatermark`, version 1)

A saved watermark (FEATURES X10, mock `watermark.html`). The app reads and writes it (text is
drawn with Qt, so this kind stays out of libpixelfx); it uses the envelope of section 1 with
`kind: "watermark"`. Kept in `<AppData>/watermarks`, one file each, named after the
watermark; a logo is copied beside it under the same name, and only its file name is stored,
so a watermark never points outside its folder.

```json
{
  "format": "pixelfx",
  "kind": "watermark",
  "version": 1,
  "name": "Studio signature",
  "body": {
    "type": "text",
    "text": "© {year} Keshav Bhatt",
    "logo": "",
    "font": "",
    "colorMode": "auto",
    "color": "#ffffff",
    "size": 0.03,
    "opacity": 0.7,
    "margin": 0.02,
    "anchor": 8,
    "position": { "x": 0.5, "y": 0.9 },
    "repeat": false,
    "angle": -30,
    "shadow": true
  }
}
```

- `type`: `text` or `logo`. `text` may hold `{year}`, `{date}` (ISO) and `{camera}`, filled
  per photo from its Exif (today's date when it has none). `font` empty means the system's
  sans; text is drawn semibold.
- `colorMode`: `auto` (white over dark areas, near black over light ones, whichever has the
  higher contrast against the mean luminance under the mark), `white`, `black` or `custom`
  (`color`). A logo keeps its own colours.
- `size` is the mark's height and `margin` its distance from the edges, both as shares of the
  photo's long edge, so the mark looks the same on every photo of a batch. `size` is kept in
  0.005 to 0.25, `margin` in 0 to 0.2, `opacity` in 0 to 1; values outside are clamped.
- `anchor`: 0 to 8, rows of three from the top left (8 is bottom right). `position`, when
  present, is the mark's centre as shares of the width and height (dragged on the preview)
  and wins over `anchor`; the mark is kept inside the photo.
- `repeat` tiles the mark over the photo in offset rows turned by `angle` degrees (-90 to 90).
- Applied on export after resizing, so it is sized for the final pixels; never on the editor
  view, never stored in a photo's settings. Batch recipes carry it embedded (section 4,
  version 2).
