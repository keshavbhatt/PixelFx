# Presets, looks, recipes and batch recipes: the file formats

Status: DRAFT for owner review (M0). Becomes binding with ADR-005 once approved; from then on
any change bumps `version` and adds a migration in `libs/pixelfx-core/src/recipe/migrate.cpp`.

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
  "color":   { "wb": { "mode": "relative", "temperature": 14, "tint": 0 }, "vibrance": 22, "saturation": 0 },
  "mixer":   { "hue": { "yellow": -8 }, "saturation": { "green": -30 }, "luminance": {} },
  "curves":  { "rgb": [[0,0],[0.28,0.2],[0.72,0.76],[1,1]], "r": null, "g": null, "b": null },
  "grading": { "shadows": { "hue": 200, "amount": 20 }, "highlights": { "hue": 35, "amount": 15 }, "balance": 10 },
  "detail":  { "clarity": 15, "sharpen": { "amount": 30, "radius": 0.0004, "masking": 0 }, "denoise": { "luminance": 0, "color": 25 }, "dehaze": 0 },
  "effects": { "vignette": { "amount": -22, "midpoint": 50, "roundness": 0 }, "grain": { "amount": 0, "size": 25 }, "fade": 0 },
  "geometry":{ "crop": { "x": 0.04, "y": 0.03, "w": 0.88, "h": 0.87 }, "angle": 1.5, "rotate90": 0, "flipH": false, "flipV": false,
               "perspective": { "vertical": 0, "horizontal": 0, "corners": null } },
  "look":    { "id": "builtin:golden-hour", "strength": 72 }
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
- `look.id` refers to a look by id: `builtin:<slug>` for shipped looks, a UUID for the user's.
  A recipe or batch file that must stand alone (export, sharing) embeds the look instead:
  `"look": { "embedded": { ...look body... }, "strength": 72 }`.

## 3. Look body (the user's "filter")

```json
"body": {
  "category": "color",
  "defaultStrength": 80,
  "steps": [
    { "op": "tone_curve",   "v": 1, "enabled": true,  "params": { "rgb": [[0,0.04],[0.5,0.48],[1,0.97]] } },
    { "op": "split_tone",   "v": 1, "enabled": true,  "params": { "shadowHue": 200, "highlightHue": 35, "balance": 10, "amount": 30 } },
    { "op": "hsl",          "v": 1, "enabled": true,  "params": { "saturation": { "green": -30 }, "hue": { "blue": -10 } } },
    { "op": "lut3d",        "v": 1, "enabled": true,  "params": { "resource": "portra", "mix": 60 } },
    { "op": "grain",        "v": 1, "enabled": false, "params": { "amount": 18, "size": 2 } },
    { "op": "vignette",     "v": 1, "enabled": true,  "params": { "amount": -15, "midpoint": 50 } }
  ],
  "resources": {
    "portra": { "type": "lut3d", "size": 33, "domain": "srgb-gamma", "interpolation": "tetrahedral", "encoding": "f16-base64",
                "name": "kodak-portra.cube", "sha256": "...", "data": "..." }
  }
}
```

- `op` names an operation in the registry; `v` is that operation's parameter version (each
  operation migrates its own params). An unknown `op` makes the look load with that step
  disabled and a warning, never fail as a whole.
- Steps run top to bottom on the image as it is after the photo's own Light, Color and Detail
  settings (the look position in section 5). `strength` blends the look's result with its
  input in linear light: 0 is no change, 100 is the full look.
- LUTs are embedded, so a `.pfxlook` is one self-contained file. `domain` (the encoding the
  LUT expects as input) is mandatory, because a LUT applied in the wrong encoding is the
  commonest LUT bug; `sha256` detects corruption and deduplicates imports.
- Each step may later carry `"mask"` (local adjustments, FEATURES E19); the key is reserved. Imported `.cube` (1D and 3D,
  `DOMAIN_MIN` / `DOMAIN_MAX` honoured) and HaldCLUT PNG become `lut1d` / `lut3d` resources.
- "Bake to .cube" evaluates the colour-only steps (those the registry marks `pointwise`) on a
  33-point grid; spatial steps (grain, vignette, clarity) are listed as left out.

## 4. Recipe (sidecar) and batch bodies

```json
"body": {
  "source": { "name": "sunset-lake.jpg", "size": 8812341, "fingerprint": "xxh3:9f1c0a5be2d47a10" },
  "settings": { ...all groups... },
  "history": [ { "label": "Shadows +28", "settings": { "light": { "shadows": 28 } } } ],
  "snapshots": [ { "name": "Warmer", "settings": { ... } } ]
}
```

- The fingerprint (size plus a hash of the first and last 64 KiB) re-attaches a sidecar to a
  photo that was moved or renamed within the library folder.
- History entries store only the changed groups (deltas), capped at 200 steps per photo.

```json
"body": {
  "stack": [ { "look": { "id": "builtin:golden-hour", "strength": 72 } }, { "preset": { "embedded": { ... } } } ],
  "autoFirst": false,
  "keepExistingEdits": false,
  "resize": { "mode": "longEdge", "value": 2048, "noEnlarge": true },
  "output": { "format": "jpeg", "quality": 90, "colorSpace": "srgb", "metadata": { "camera": true, "location": false },
              "folder": "~/Pictures/PixelFx/Batch", "beside": false, "name": "{name}-{preset}", "ifExists": "number" }
}
```

- The stack is applied in order onto each photo's settings (later entries win per key).
  A batch file always embeds user presets and looks so it runs on another machine;
  `builtin:` references stay references.

## 5. Pipeline order (fixed, documented here and in ADR-004)

Input decode and orientation, RAW development, then: geometry (crop, rotate, perspective, as
one resampling) -> noise reduction -> white balance -> exposure -> dehaze -> tone (highlights,
shadows, whites, blacks, contrast) -> clarity -> curves -> colour mixer -> vibrance and
saturation -> colour grading -> **look** (blended by strength) -> sharpen -> vignette -> grain
-> fade -> output transform (colour space, bit depth). Identity settings are skipped.

## 6. Imports

| Source | Becomes | Mapping |
|---|---|---|
| `.cube` (1D, 3D), `.3dl`, HaldCLUT `.png` | look with one LUT step | exact |
| RawTherapee `.pp3` | LATER (subset) | |
| Lightroom / Camera Raw `.xmp` preset (`crs:` namespace) | preset | Exposure2012, Contrast2012, Highlights2012, Shadows2012, Whites2012, Blacks2012, Temperature/Tint (relative), Vibrance, Saturation, Clarity2012, Dehaze, HSL (8 bands), ToneCurvePV2012 (and per channel), SplitToning / ColorGrade, Sharpness, LuminanceSmoothing, ColorNoiseReduction, PostCropVignette, Grain; approximate, a report lists every setting skipped (profiles, masks, lens and camera calibration) |
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
  view, never stored in a photo's settings. Batch recipes (section 4) do not carry a watermark
  yet: that is a batch format version 2.
