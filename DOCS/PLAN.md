# PixelFx 2: Project Plan

> Source of truth for architecture and scope. Sections marked **DECISION** are settled; do not
> re-litigate without asking the owner. Section 9 lists **OPEN** questions for the owner.

---

## 1. Goal

A quick, cross-platform (Linux first, then Windows and macOS) photo retouching and
manipulation app in Qt 6 / C++, built on a reusable open-source image library. Open a photo and
see it immediately; make it better with sliders, one-click looks and saved presets while
watching it change; build your own looks and presets; apply any of them to hundreds of photos
at once; export without ever touching the original.

Reference points: the editing model of Lightroom and darktable (non-destructive settings, a
fixed pipeline), the speed and focus of Apple Photos and Snapseed (one window, few modes), the
batch power of XnConvert. PixelFx is not a raster editor (no layers, text or painting) and not
a catalogue manager (no import step, no database of photos).

### Non-goals (2.0)

- Layers, compositing, text, drawing (FEATURES E21: DROP).
- A photo catalogue with ratings, keywords and collections.
- Local masks and healing (LATER: E19, E20), lens profiles (LATER: G6), AI tools (LATER: E22).
- Mobile builds.

---

## 2. Key decisions (DECISION)

| # | Decision | Rationale |
|---|---|---|
| D1 | Monorepo: `libs/pixelfx-core` (libpixelfx) and `app/` (owner, 2026-11-04; ADR-001) | One change can touch both; the library can be split out with `git subtree` when it has outside users |
| D2 | libpixelfx is standalone C++20 with no Qt, Apache-2.0 (D13), namespace `pfx` (owner, 2026-11-04) | Reusable by any program, including non-Qt and closed ones; replaces the Go tool |
| D3 | The app is GPL-3.0-or-later with the Ktechpit licensing module under its own licence, REUSE compliant (kit Model B; owner, 2026-11-04) | Same as Omnidict and Playlist Downloader |
| D4 | Identity: name PixelFx, id `com.ktechpit.pixelfx`, snap name `pixelfx` (kept so existing installs update), org `ktechpit`, developer Keshav Bhatt (owner, 2026-11-04) | Continuity for v1 users |
| D5 | Images are processed in-process; no helper process, no shell (ADR-002) | v1's per-apply process was the root of its latency and silent failures |
| D6 | Non-destructive editing: a photo's edit is a recipe of settings, rendered by a fixed pipeline in float linear light; the original file is never written (ADR-004) | Undo, history, presets, batch and sidecars all fall out of it |
| D7 | Every operation is registered once with a parameter schema; the UI, presets, looks, imports and the CLI all read the registry (ADR-005) | One source of truth for names, ranges, units and versions |
| D8 | One JSON envelope for recipe, preset, look and batch files (`DOCS/presets.md`) | Shareable, diffable, versioned with migrations |
| D9 | Qt Widgets UI, no QML, no `.ui` files; Qt 6.11 built against `kde-qt6-core24-sdk`, run against `kf6-core24` (ADR-000) | The kit's proven stack and dev/prod parity |
| D10 | Snap Store and Flathub first (Flathub from the snap); Windows and macOS build in CI from M2, store releases after the Linux 2.0 (D19) | Owner's stated targets |
| D11 | Mocks before screens, with a written review per mock (DESIGN section 5) | The owner's process for every app |
| D12 | The design is new; nothing from v1's layout is kept (owner, 2026-11-04) | v1's layout was a toolbar, a dock and floating dialogs |
| D13 | libpixelfx is Apache-2.0 (owner, 2026-11-04; Q1) | Permissive with an explicit patent grant, which matters around image codecs |
| D14 | 2.0 renders on the CPU; a GPU preview through Qt RHI follows in 2.1 (Q2) | Smaller 2.0, no GPU driver issues in snaps; the CPU path meets the 30 fps target |
| D15 | Camera RAW ships in 2.0 through LibRaw (Q3) | The main reason photographers would choose PixelFx over a simple editor |
| D16 | What the licensing module gates (owner, 2026-11-04): recorded in the private notes, `DOCS/private/business.md` | Business decision, not published |
| D17 | The icon is redrawn as a vector in the colour-wheel spirit (Q5) | Crisp at 16 px, a dark-background variant for Flathub |
| D18 | Edits are kept in the app library by default; next-to-photo is a setting (Q6) | No clutter in photo folders, works on read-only media |
| D19 | Windows and macOS build in CI from M2; their store releases follow the Linux 2.0 (Q8) | Nothing Linux-only creeps in, the first release stays focused |
| D20 | The rewrite lives in `github.com/keshavbhatt/PixelFx` with the old main archived on a branch; `pixel` becomes read-only (Q9) | Keeps the history, stars and links |
| D21 | PixelFx's keywords leave batch-resize and conversion searches to PixelBatch, FormatFactory and others (Q10) | Avoids competing with the owner's own apps |
| D22 | Store text: name `PixelFx`, summary `Photo editor, filters and presets`, keywords `retouch`, `heic`, `raw` (Q11) | Flathub naming rules and the measured field (keyword research in the private notes) |
| D23 | What the licensing module gates (owner, 2026-11-04): recorded in the private notes, `DOCS/private/business.md` | Business decision, not published |
| D24 | PixelBatch is folded into PixelFx (owner, 2026-11-04; Q10b): PixelFx reaches feature parity for optimisation (FEATURES M10 to M13), then PixelBatch gets a final update that points its users to PixelFx; on Flathub, `com.ktechpit.pixelbatch` is marked end-of-life with `end-of-life-rebase` to `com.ktechpit.pixelfx` | One app to maintain; PixelFx's Batch page does everything PixelBatch does |
| D25 | Every mock in `mocks/index.html` as of 2026-11-04 is approved (owner; Q7) | Screens are coded from them when their milestone comes; later changes get a new mock and review |
| D26 | What the licensing module gates (owner, 2026-11-04): recorded in the private notes, `DOCS/private/business.md` | Business decision, not published |

---

## 3. Repository layout

```
pixelfx/
  CLAUDE.md  README.md  CHANGELOG.md  LICENSING.md  REUSE.toml  THIRD_PARTY.md  Makefile
  .clang-format  .clang-tidy  .editorconfig  .gitignore  LICENSES/
  DOCS/                      this plan, contract, ADRs, design, formats, mocks, research, reference
  libs/pixelfx-core/         libpixelfx (own CMakeLists.txt, LICENSE, README, CHANGELOG)
    include/pfx/             image.h, color.h, recipe.h, registry.h, pipeline.h, io.h, batch.h, result.h, document.h
    src/{image,color,ops,io,recipe,batch}/
    tools/pfx/               pfx command line tool
    tests/                   doctest; fixtures/, golden/, fuzz/
  app/
    CMakeLists.txt           add_subdirectory(../libs/pixelfx-core)
    src/{app,core,services,platform,models,ui,modules/AccountAndLicense}/
    tests/
  presets/                   built-in looks and presets (.pfxlook, .pfxpreset)
  scripts/                   dev-build.sh, dev-run.sh, snap-runtime-env.sh, screenshots.py
  snap/snapcraft.yaml  packaging/ (desktop, metainfo, flatpak)  .github/workflows/
```

The old repositories stay frozen as reference: `PixelFx/` and `pixel/` beside this tree are not
part of it.

---

## 4. Vocabulary and domain model

| Word in the UI | In code | Meaning |
|---|---|---|
| Adjustment | `pfx::OpSpec` + a settings key | One slider or control |
| (an edit) | `pfx::Recipe` | Every setting of one photo; stored in a sidecar |
| Look ("filter") | `pfx::Look` | Ordered steps (operations, LUTs) applied with one strength |
| Preset | `pfx::Preset` | A partial recipe (ticked groups), optionally with a look |
| Batch recipe | `pfx::BatchRecipe` | A stack of presets and looks plus resize and output |
| Photo (open) | `pixelfx::services::Document` | A decoded source, its recipe, history and caches |

The UI never says recipe, pipeline, op or step parameter (DESIGN section 5, lens 5).

---

## 5. libpixelfx architecture

### 5.1 Data

- `pfx::Image`: float32 RGBA, linear light, in the linear Rec.2020 working space (ADR-004),
  owned buffer with row stride, or a tiled view; backed internally by a libvips image (ADR-003).
- `pfx::Settings` / `Recipe` / `Preset` / `Look` / `BatchRecipe`: plain structs, JSON through
  `pfx::Document` (`presets.md`), migrations per version.
- `pfx::OpSpec`: id, display name key, group, parameters (`ParamSpec`: id, type, min, max,
  default, step, unit, `relative` for spatial), flags (`pointwise` for LUT baking, `needsRadius`
  for tiling margins), version, migration function.

### 5.2 Pipeline

- `pfx::Pipeline::compile(const Recipe&, const SourceInfo&)` produces an ordered list of bound
  operations in the fixed order of `presets.md` section 5, skipping identities.
- `render(const RenderRequest&)`: request = region of interest in output coordinates, scale
  (1.0 for export, the screen scale for preview), and a cancellation token. Spatial parameters
  are converted to pixels for that scale, so preview and export agree.
- Tiles with margins from each operation's radius, evaluated on demand by libvips's
  pipeline, our operations running inside it through `vips_image_generate` (ADR-003). A cache keeps the output of the most recent expensive prefix (decode and
  geometry, then the stage before the changed group), so moving a slider in Effects does not
  re-run noise reduction.
- Interactive preview path: render at screen resolution of the visible region first (one
  pass), then refine; the app cancels stale requests by generation. A GPU path for pointwise
  operations follows in 2.1 (D14).

### 5.3 Codecs, colour and metadata

Behind interfaces (`pfx::Decoder`, `pfx::Encoder`, `pfx::MetadataReader/Writer`,
`pfx::ColorEngine`) so dependencies can change without touching operations. Concrete choices
are ADR-003, from `DOCS/research/image-libraries.md` (section 5.6 below is filled from it).

### 5.4 Batch engine

`pfx::BatchRunner`: input list, a `BatchRecipe`, a memory budget (default half of RAM,
user-settable) and a thread count; about cores / 2 photos in flight, each with cores / jobs
threads, fewer when the budget says so; statistics (Auto, histograms) from a 1 to 2 MP
shrink-on-load copy, the full-resolution pass streamed through tiles to the encoder so no
full-size float image is ever resident; output byte-identical regardless of thread count
(grain seeded by pixel coordinates); streams progress and per-file results (saved
path, or an error code and message) through callbacks, supports pause and cancel. The app's
Batch page and `pixelfx --batch` both drive it; so does `pfx batch`.

### 5.5 `pfx` command line tool

```
pfx info photo.jpg
pfx apply --preset moody.pfxpreset --look golden.pfxlook -o out/ *.jpg
pfx batch recipe.pfxbatch ~/Pictures/trip
pfx convert --to avif --quality 60 --long-edge 2048 in.heic out.avif
pfx lut bake look.pfxlook -o look.cube
pfx ops            # lists the registry: ids, ranges, units (also the source for the docs)
```

### 5.6 Library stack (ADR-003, from `research/image-libraries.md`)

| Need | Choice | Licence |
|---|---|---|
| Adjustment and look operations | Our own C++20 code, SIMD with Google Highway | Apache-2.0 / BSD-3 |
| Decode, encode, shrink-on-load, resample, warps, tiled streaming, async preview tiles | libvips (reduced build) | LGPL-2.1-or-later, dynamic |
| Colour management | LittleCMS2 (no fast_float plugin) | MIT |
| RAW | LibRaw | LGPL-2.1 or CDDL-1.0, dynamic |
| Codecs | libjpeg-turbo, libpng, libtiff, libwebp, libheif + libde265 + dav1d, libavif, libjxl; OpenEXR optional | permissive / LGPL |
| Metadata | Exif, XMP, ICC blobs through libvips (libexif) and codec boxes; no Exiv2 | LGPL |
| GPU preview (2.1, D14) | Qt RHI in the app, CPU authoritative | Qt |
| Tests | doctest, libFuzzer | MIT / Apache-2.0 |

---

## 6. App architecture

- `app/`: `Application` (identity, CLI, single instance forwarding files, `--batch`), main.
- `core/`: `Settings` facade; `SidecarStore` (recipe files in the app library keyed by
  fingerprint, or `.pfx` next to the photo); `PresetLibrary` (built-in resources plus the user
  folder `AppDataLocation/presets`, watched for changes); `History` (undo stack of setting
  deltas, snapshots).
- `services/`: `Document` (one open photo: source, recipe, history); `RenderScheduler` (one
  worker thread that owns the interactive pipeline, coalesces slider moves to the latest value,
  cancels stale generations, emits `previewReady(QImage, generation)`); `ThumbnailRenderer`
  (look tiles and filmstrip at small size, low priority); `ExportService`; `BatchService`
  (wraps `pfx::BatchRunner`); `XmpPresetImporter`, `LutImporter`; `qt_image_bridge`.
- `models/`: `FilmstripModel` (selection), `LooksModel`, `PresetsModel`, `BatchQueueModel`,
  `HistoryModel`.
- `platform/`: portal file chooser and OpenURI, notifications, display colour profile
  (colord on Linux, ColorSync on macOS, WCS on Windows), crash handler.
- `ui/`: `MainWindow` (owns controllers), `Canvas` (QWidget painting the preview with zoom and
  pan; crop overlay), `LooksPanel`, `PresetsPanel`, `AdjustPanel` (built from the registry,
  grouped as DESIGN), `CropPanel`, `BatchPage`, `ExportSheet`, `PresetSaveSheet`,
  `LookEditorSheet`, `SettingsSheet`, kit sheets (shortcuts, about, what's new, bug report),
  `pixelfx_style` tokens from `DESIGN.md`.

Threads: the GUI thread paints; the render scheduler's worker owns interactive rendering; the
library's pool does tiles; batch has its own pool limited so the editor stays responsive
(ADR-006).

---

## 7. Performance targets (measured on the owner's machine and a 4-core laptop)

| Case | Target |
|---|---|
| Open a 24 MP JPEG: first pixels on screen | under 150 ms (embedded preview or fast downscale) |
| Open a 24 MP JPEG: full-quality fit view | under 600 ms |
| Slider drag, fit view at 2560 by 1440 | 30 fps or better on CPU; 60 fps with the GPU path |
| Look tiles (24) rendered for a new photo | under 1 s, progressively |
| Export 24 MP JPEG with Light, Color, Detail and a look | under 1.5 s |
| Batch, 500 photos of 24 MP to 2048 px JPEG, 8 threads | under 3 minutes, memory within the budget |

---

## 8. Platforms and packaging

- Linux: snap (core24, kde-neon-6 extension, amd64 and arm64, built by GitHub Actions) and
  Flathub (the snap extracted, KDE runtime). Codec libraries the runtimes lack are built as
  parts and modules at pinned versions.
- Windows (MSVC, vcpkg for the library's dependencies, MSIX or Inno installer) and macOS
  (universal, notarised DMG) build in CI from M2; their store releases follow the Linux 2.0 (D19).
- MIME types for every supported input; `Exec=pixelfx %F`; the snap needs `home` and
  `removable-media` (photos on cards and external drives), nothing else beyond the desktop set.

---

## 9. Owner questions

Answered 2026-11-04: the owner accepted the recommendations below and then answered the three
that had none. The decisions are D13 to D26 in section 2.

| # | Question | Decision |
|---|---|---|
| Q1 | Library licence | **Apache-2.0** (patent grant; every dependency is permissive or LGPL, ADR-003) |
| Q2 | GPU preview in 2.0? | **CPU first** in 2.0 (libvips-driven, preview-scale, SIMD); GPU through Qt RHI in 2.1 for the pointwise operations, CPU authoritative |
| Q3 | Camera RAW in 2.0? | **Yes**, through LibRaw |
| Q4 | A licensing question | Answered; recorded in `DOCS/private/business.md` |
| Q5 | App icon | **Redraw as a vector** in the colour-wheel spirit, with a dark-background variant |
| Q6 | Where edits live | **App library by default**, a `.pfx` file next to the photo as a setting |
| Q8 | Windows and macOS | **Built in CI from M2**; store releases after the Linux 2.0 |
| Q9 | Repository | **Reuse `github.com/keshavbhatt/PixelFx`**, old main archived on a branch; `pixel` made read-only |
| Q10 | PixelBatch overlap | **PixelFx does not target** "batch resize", "image converter" or "convert images" in its keywords |
| Q11 | Store text | **Name `PixelFx`, summary `Photo editor, filters and presets`, keywords `retouch`, `heic`, `raw`** |

Answered without a prior recommendation (same day):

| # | Question | Decision |
|---|---|---|
| Q4b | A licensing question | Answered; recorded in `DOCS/private/business.md` |
| Q7 | Mock approval | **All approved** (D25) |
| Q10b | Fold PixelBatch into PixelFx or keep it? | **Fold it in** (D24) |

| Q12 | A licensing question | Answered; recorded in `DOCS/private/business.md` |

Nothing is open.

---

## 10. Milestones

See `ROADMAP.md`: M0 analysis and contract, M1 library core, M2 app skeleton, M3 editor, M4
looks and presets, M5 batch, M6 polish and text, M7 packaging and release.
