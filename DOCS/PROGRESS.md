# Progress

## Milestones

| Milestone | Status |
|---|---|
| M0 Analysis and contract | done |
| M1 Library core | done (2026-11-04), performance carried into M3 |
| M2 App skeleton | done locally (2026-11-04); Windows and macOS CI written, first run after the next push |
| M3 Editor | wip (live editing, undo, before and after started 2026-11-05) |
| M4 Looks and presets | wip (presets, Lightroom import, looks, the look editor, LUTs: 2026-11-05) |
| M5 Batch | wip (Batch page, runner, recipes, watermark: 2026-11-05) |
| M6 Polish and text | todo |
| M7 Packaging and release | todo |

## Mock approvals

| Mock | Reviewed | Owner |
|---|---|---|
| `editor.html` | 2026-11-04 (`mocks/reviews/editor.md`) | approved 2026-11-04 |
| `heal.html`, `watermark.html`, `export-sizes.html` | 2026-11-04 (`mocks/reviews/`) | approved 2026-11-04 |
| every other mock in `mocks/index.html` | written review still owed | approved 2026-11-04 |
| `batch-compress.html` | 2026-11-04 (`mocks/reviews/batch-compress.md`) | approved 2026-11-04 |
| `editor-grading.html` | 2026-11-05 (`mocks/reviews/editor-grading.md`) | approved 2026-11-05 (as drawn: two wheels, a group of its own) |
| `about.html`, `whats-new.html` | 2026-11-05 (`mocks/reviews/about-whats-new.md`) | approved 2026-11-05 |
| `editor-value-entry.html` | 2026-11-05 (`mocks/reviews/editor-value-entry.md`) | approved 2026-11-05 |
| `preview-viewer.html` | 2026-11-05 (`mocks/reviews/preview-viewer.md`) | approved 2026-11-05 (built with the editor's zoom bar; the counter in the title) |
| `editor-develop.html` | 2026-11-05 (`mocks/reviews/editor-develop.md`) | approved 2026-11-05 (supersedes the two-wheel grading of `editor-grading.html`) |
| `editor-presets-catalog.html` | 2026-11-05 (`mocks/reviews/editor-presets-catalog.md`) | approved 2026-11-05 |
| `editor-mixer-target.html` | 2026-11-05 (`mocks/reviews/editor-mixer-target.md`) | approved 2026-11-05 |
| `editor-collapse-all.html` | 2026-11-05 (`mocks/reviews/editor-collapse-all.md`) | approved 2026-11-05 |
| `editor-zoom-loading.html` | 2026-11-05 (`mocks/reviews/editor-zoom-loading.md`) | approved 2026-11-05 |
| `editor-looks-preview.html` | 2026-11-05 (`mocks/reviews/editor-looks-preview.md`) | approved 2026-11-05 |
| `editor-zoom-loading.html`, slow preview | 2026-11-05 (same review, "Slow preview") | approved 2026-11-05 (the ring always turns) |
| `editor-presets-catalog.html`, hover preview | 2026-11-05 (same review, "Hover preview, reworked") | approved 2026-11-05 |

## Waiting for the owner (2026-11-05)

Decisions the work stopped at; everything else went ahead within the approved mocks.

1. Built-in looks (13) and presets (4): the values are drafts by eye; tune or replace them.
2. Kelvin white balance presets (Daylight, Cloudy, Shade, Tungsten in the mock) are left out:
   the engine's white balance is relative; keep them out or add absolute Kelvin.
3. The theme applies at the next start (icons are tinted when made). Accept, or ask for a live
   switch.
4. macOS CI skips one curve-editor test (a crash only on the runner, clean under ASan locally).
5. Real Lightroom `.xmp` exports should be tried on the importer (EmulsionPresets' 451 so far,
   2026-11-05; a second, real-world set is still wanted for the 95% target of FEATURES P8).
6. Batch writes beside each original into a folder named "PixelFx": keep the name?
   (The catalog's names and Tools: decided 2026-11-05, see below.)
7. Zoom has no FEATURES row; add one (it is built).
8. Small departures from approved mocks, until their parts exist: no Favorites chip; the look
    editor previews on This photo and a colour ramp (no Portrait or Landscape test photos are
    shipped); the main menu leaves out Save edit with the file and Online guide (item 10);
    Copy to clipboard has no shortcut (the mock's
    Ctrl+Shift+C is Copy settings).
9. Still to plan: monitor profile, GPU preview, sidecars next to the photo, the batch-end
    notification (needs Qt D-Bus on Linux, a dependency decision).
10. Online guide: the main menu and About leave it out until there is a guide to point at.
    Where should it go (a page on ktechpit.com, the GitHub wiki, the README)?
11. Small choices made with the approved About, What's new and grading mocks, easy to change:
    What's new does not open on a first install (nothing is new yet), only after an update from
    an earlier version, PixelFx 1 included; Diagnostics leaves out "memory for previews" (there
    is no such limit yet); the Color grading group starts closed, like the Color mixer; the
    open-source notices list what PixelFx ships (THIRD_PARTY.md), not the licensing module.
12. Small choices made in Heal, easy to change: while Heal is on, the photo shows upright and
    uncropped (spots live in that frame, so a later crop never moves them; the mock shows no
    crop either way); spots are named "Spot 1", "Brush stroke 2" rather than by place ("Spot in
    the sky" in the mock needs scene recognition); Reset takes off the spots made since Heal
    opened and Done keeps them (each spot is also its own history step, and undo works in
    Heal); Reveal dust explains itself in its tooltip rather than a first-time line.

## Sessions (newest first)

### 2026-11-06 (Batch: Compress only)

- The approved `batch-compress.html`, built (FEATURES M10, M11, part of M12). libpixelfx gained
  `pfx/compress.h` and `pfx compress`: lossless JPEG through libjpeg-turbo's coefficient API,
  our own PNG writer (smallest colour type and bit depth, filters, palettes, every chunk kept in
  its place), our own palette quantiser with pngquant's quality scale (owner's choice over
  quantizr, which needs Rust in every build), our own GIF writer, TIFF and lossless WebP stored
  again without loss, location and previews removed from Exif, XMP, Photoshop and WebP chunks.
  ADR-010; THIRD_PARTY gained libjpeg-turbo and zlib (already in every runtime).
- Batch has "1. What to do" at the top: Edit and export (sections now 2 to 5) or Compress only
  (`CompressPanel`, `CompressList`, `CompressCompare`; `StatusPill` now shared by both lists).
  The runner compresses up to 4 files at once, replaces what the last run of the same list
  wrote, and keeps the original's most detailed spot when the originals are replaced. The mode
  and the choices are remembered, Replace the originals is not.
- Changed while building, recorded in the mock and `reviews/batch-compress.md`: the "Others"
  line now says only what is done (no AVIF encoder; a lossy WebP is never encoded again), the
  row's ⋯ became ×, a JPEG quality slider for Target quality.
- On copies of the e2e photos: JPEGs 6 to 14% smaller without loss in about 0.1 s each, a 3.7 MB
  PNG photo 1.9 MB lossless or 1.0 MB with fewer colours (quality 96).
- Owner's DNG from Samsung Expert RAW (Galaxy S22 Ultra) did not open: it is DNG 1.7, linear
  (already demosaiced) and JPEG XL compressed, which LibRaw reads only with Adobe's DNG SDK. The
  message is now plain words. For FEATURES O3: decode the JPEG XL tiles with libjxl (already a
  dependency), then black and white levels, AsShotNeutral, ColorMatrix1, BaselineExposure (+3
  in that file) and its ProfileGainTableMap (Samsung's local tone map). The file also holds a
  full-size JPEG preview.
- Checks: 145 library tests (new: every lossless method decodes to identical pixels), 36 app
  suites (new: `tst_batch_compress`, Compress only in `tst_batch`), the linters; the real app
  headless on six mixed files matches the mock.

### 2026-11-05 (a slow preview)

- The approved slow preview of `editor-zoom-loading.html`: the Preview tag and the line come
  with the previewed picture, not when it is asked for, so the photo never claims a preview it
  does not show yet. `Document::HoverState` (Off, Preparing, Shown) reports it; past 0.3 s the
  chip says "Preparing preview", zoomed in or not, then "Updating" while the sharp picture of the
  preview renders. A render made for a tile already left is dropped, and so is one for a preview
  already ended.
- Owner: the ring always turns. `BusyRing` no longer stands still when the desktop turns
  animations off, and `animationsWanted` went with it (nothing else used it).
- Build: the host's Qt went to 6.12 and the dev build broke, because CMake had been taking
  moc, rcc and uic from the host (it searches the SDK's `lib/x86_64-linux-gnu` only on a
  Debian-style host). `scripts/dev-build.sh` now finds Qt's tool packages in the SDK and drops
  any found on the host before; the snap and Flatpak builds are not affected.
- Checks: 35 app suites, 134 library tests, the linters; the real app headless with a 6021 by
  3422 photo shows "Preparing preview" and no tag until the preview arrives, as in the mock.

### 2026-11-05 (Looks, hover preview)

- The approved `editor-looks-preview.html`: resting on a look previews it as presets do (the look
  at its own strength over the edit, in place of the applied one; Original as no look), and
  "Applied" above Look strength names the look the slider belongs to ("None" greys it).
- One `HoverPreview` now runs the wait, the bar, the quicker next tile and the moment's grace on
  leaving for both tabs; `MainWindow::previewOnPhoto` shows either on the photo (the Preview tag,
  the line, Escape), so there is one copy of the behaviour.

### 2026-11-05 (zoom: loading the sharp picture)

- The approved `editor-zoom-loading.html`: zoomed in past the preview, a chip beside the zoom bar
  says "Loading" while the photo is read at full size and "Updating" while an edit is redrawn,
  once the wait passes 0.3 s; the bar itself never moves. `Document::FullState` reports where the
  full-size picture is; `BusyRing` turns (it stood still when the desktop wanted no animation until the
  owner asked for it always on, below); screen readers hear it as an announcement. The full-screen viewer shows
  the chip when going to the next photo.
- Owner's finding: zoomed in, a preset's preview did not show (only the small preview and the
  histogram changed, the sharp picture of the edit stayed on top). A preview now comes after a
  deliberate wait, so it is drawn at full size too; when it ends, the edit's sharp picture is
  kept and comes straight back.

### 2026-11-05 (hover preview, Collapse all)

- Owner's findings: opening a category in full showed a count of 0 and no pictures past the
  first six (new tiles of a shown panel count as hidden until the next event loop pass, so the
  panel now keeps its own list of the tiles it built); a preset previewed the instant the pointer
  touched it; the notices list's highlights overlapped (rows were laid out before the style sheet
  padded them; every item view now lays its rows out again once styled).
- The approved hover preview: 0.35 s resting on a tile, 0.15 s once a preview shows, a bar
  filling along the bottom of the tile's picture meanwhile; a Preview tag on the photo and the
  line "Preview of ..." at the top (`paintCanvasBanner`, shared with the colour picker's line);
  Escape, a click, or leaving the tiles ends it; keyboard focus previews as hovering does.
- The approved Collapse all: a button at the end of the adjust panel's top row and in the
  Presets search row; a heading's menu with Collapse all, Expand all, One group open at a time
  (remembered) and Open only <group>; Alt+click on a heading opens only that group.

### 2026-11-05 (a colour picked on the photo)

- The approved `editor-mixer-target.html`: the target at the end of the Color mixer's swatches
  (T, and in the Photo menu) picks the colour under the pointer and a vertical drag changes it in
  the mode on show, or in the B&W mix; a colour between two bands moves both by their shares,
  the panel names the second ("Also moves Aquas hue +7"), one drag is one history step. The
  colour is read before the mixer (`pfx::settingsBefore`, `Document::colorBeforeMixer`, a 640 px
  render cached until an earlier setting changes) and its bands come from the library
  (`pfx::hueBands`), as the mixer weighs them. Escape, T, Crop, Heal and Batch stop it.
- The CHANGELOG now tells today's work: Lightroom presets in full, the catalog, black and white,
  colour grading in four ranges, Texture, Regions, Optics and Calibration.

### 2026-11-05 (the preset catalog)

- The Presets tab of the approved `editor-presets-catalog.html`: search, All, Favorites (F),
  Mine and Recent, the user's groups, then Essentials and the twelve catalog categories as tiles
  of the photo, six per open category with "All N" for the whole of one, categories closed by the
  user remembered, a tile's menu (Apply, Apply to the selection, Favorites, Copy to My presets,
  Export), and the Amount of the preset just applied (0 to 200, as Lightroom's): it blends from
  the photo before the preset, a setting moved by hand since keeps the user's value, and undo
  takes it away with its step. Only the tiles on show get their picture rendered.
- The catalog: 429 of EmulsionPresets' 451 (MIT, ADR-009), converted by `scripts/catalog/
  build_catalog.py` with the new `pfx import`, renamed without brands or people
  (`scripts/catalog/catalog.tsv`, a test checks it), described from their settings
  (`core::describeSettings`). On the 33 ColorWall photos every one renders and, outside Tools,
  visibly changes the photo.
- Shared pieces: `PhotoTile` and `tileGrid` for the Looks and Presets tabs, `core::hueName`.
- Decided by the owner: the names are fine; Tools stays, last in the list.

### 2026-11-05 (Lightroom compatibility, the panel)

- The panel of the approved `editor-develop.html`: Color or Black & white at the top of Color
  (V, also in the Photo menu; off while healing, where V shows the spots), Profile, the Color
  mixer turning into the B&W mix, Color grading with four small wheels choosing the range and
  one large wheel editing it, Curves with Points and Regions (split marks dragged or moved with
  the keys), Texture, a "More" per group remembered between sessions, and the new Optics and
  Calibration groups. Black & white, Optics and Calibration are parts of presets, pastes and
  batch recipes, and a colour preset now turns a black-and-white photo back to colour.
- Shared pieces: groups are built from a table of controls (sliders, choices, checks, with
  sub-headings and More), `ChoiceRow` and `CheckRow`, `GradingRangeButton` drawing the same wheel
  as `ColorWheel` (`paintColorWheel`), `RegionCurveView` drawing the library's own sampler, and
  `SegmentFit` for segmented controls that share their row.
- Small fixes on the way: a "&" in a button's label no longer becomes a shortcut letter; values
  that cannot go below zero show no plus sign; every coloured track has a rim; disabled slider
  rows look disabled; the colour mixer stands aside in black and white, as in Lightroom.
- Next: the preset catalog (`editor-presets-catalog.html`) and the curation of the 451 presets.

### 2026-11-05 (Lightroom compatibility, library)

- Decided by the owner: PixelFx reads and renders every global develop setting a Lightroom
  preset uses (FEATURES P8), each shown as a control; secondary sliders sit under a "More"
  disclosure in their group; colour grading gets midtones and global (the approved grading mock
  is reopened); lens profiles wait for their own ADR (lensfun). A large built-in preset catalog
  follows (P9), browsed as tiles with an Amount slider (mock first).
- Measured on EmulsionPresets (451 MIT-licensed Lightroom presets, 103 settings): before, 413
  presets lost a setting on import, the 83 black-and-white ones came in as colour. Now 449 import
  with nothing skipped; the other 2 switch on a lens profile.
- libpixelfx: black and white with the B&W mix; colour grading with midtones, global, a
  luminance per range and blending; Texture; Calibration (primaries, shadow tint); the vivid
  profile; the parametric curve with its splits; sharpen detail; noise reduction detail,
  contrast and smoothness; vignette feather, style and highlights; grain roughness; lateral
  chromatic aberration measured on the photo; defringe. Every default renders as before (the
  golden images are unchanged); each new setting has a golden image, a scale test and a
  behaviour test. The importer maps all of them, profiles included.
- Formats: presets version 2, recipes and batch recipes version 3, older files read unchanged.
- Next: the app's preset parts for B&W and Calibration (they change the Save as preset and Paste
  sheets), the controls mock, then the catalog mock.

### 2026-11-05 (UI against the mocks)

- Decided by the owner, then changed the same day: sheets have no system title bar. They are
  drawn as the mocks draw them (rounded, bordered, over a dimmed window) and moved by dragging
  them; the system's own dialogs (file and colour pickers, short questions) stay native.

- Sheets against their mocks, one at a time: Export, Watermark, Save as preset, Paste settings,
  Settings, Keyboard shortcuts, About, What's new and the look editor. Shared pieces came out of
  it: `Sheet` (frameless, over a dimmed window), `PartChecklist` (the parts of a preset or a
  paste with what changed), `StepList` (reorderable steps), `ElidedLabel`, `setLabelColumn`
  (muted form labels in one column), `coverThumbnail`, `icons::toggle`. Sliders anywhere are
  `SliderRow`s, so every value can be typed and shows in the accent when changed.
- Left for later, from the mocks (drift list, section B): Export's Sharpen for, Color space and
  size estimate; Paste's Replace their settings; Settings' other pages; the watermark font
  weight. The mocks are drawn at 13 px; PixelFx uses the system font, which reads near 12 px
  on the owner's desktop.
- The owner found the Batch page far from `batch.html` and raw Fusion controls everywhere.
  Every screen was grabbed and set beside its mock; the gaps were shared, so they were fixed
  at the root: the style sheet now draws every standard control as `mock.css` does
  (checkboxes, radio buttons, switches, combo boxes and their lists, spin boxes, scroll bars,
  tooltips, progress bars, disabled states), from glyphs made in the theme's colours at start;
  every sheet has the mocks' title row with a close button and a footer bar
  (`sheetLayout`, `sheetFootBar`, `sheetCloseButton`); switches where the mocks have them
  (Settings, Watermark, Heal); the rail has the settings gear and the header its separators.
- Batch rebuilt from `batch.html`, `batch-running.html` and `batch-done.html`: the header
  shows "Batch, 6 photos · 129 MB" with Add photos and Add folder; the list has its title row,
  a list or grid view and Clear; status pills (Saved, Saving, Waiting, Not saved); the preview
  and the running or done bar with time left or taken and the size written; the recipe as a
  card with Change (presets and looks) or Settings from a photo, Then add preset, Never
  enlarge, "JPEG, quality 90" with More... (quality, bit depth, metadata, if it exists), the
  folder as ~/..., the name with the new {preset} token, Save as batch recipe and Process at
  the foot. Open batch recipe moved to the main menu (and a .pfxbatch can be dropped on the
  page). Fixed on the way: the recipe box showed blank (its separator was selected).
- Still different from the mocks, next: Export (AVIF and More in the formats, Sharpen for,
  Color space, the size estimate, quality labels above the slider), Save as preset (the
  photo's thumbnail, the Include card in two columns with counts), the look editor's step
  rows (handle, icon, menu), Settings' other pages, the presets panel rows.

### 2026-11-05 (spot heal)

- Spot heal (E20, mock `heal.html`), approved with its format by the owner today.
- libpixelfx: a `Spots` settings value and the `heal` operation at stage 5, before geometry, so
  spots are placed on the upright photo and a later crop never moves them. `healSpot` copies
  the source in, adds the difference between the two surroundings fitted as a plane per channel
  (a spot on a sky gradient matches both sides) and feathers it; `findHealSource` tries 96
  places around the spot, keeps those inside the photo and clear of the spot, and picks the one
  whose surroundings match and whose inside looks like the spot's surroundings (so another
  blemish is not copied in). Recipes are at version 2 for `heal.spots` (presets.md sections 2,
  4, 5). Tests: a blemish on a gradient, a stroke over a line, a second blemish avoided, the
  photo's edge, render with a crop, the format, and 200 random spots on tiny and thin images;
  clean under ASan and UBSan; the document fuzzer ran 4.7 million inputs with a spots seed.
- The app: Heal on the rail (R) with its panel (Spot or Brush, size, feather, opacity, Show
  spots V, Reveal dust D, the list with Remove all, Reset and Done), the canvas draws each spot
  (white with a dark outline, the selected one with its hollow source and a dashed line), click
  to heal, drag to brush, drag a spot or its source, [ and ] for size, Delete, the arrow keys to
  nudge, Escape to leave. Each spot is a history step and undo works in Heal. Copy and paste
  never carry spots. A `CanvasTool` hook keeps the canvas free of Heal's logic; Heal's part of
  the window is in `main_window_heal.cpp`. 33 app suites and 114 library tests pass.

### 2026-11-05 (batch format version 2)

- The owner approved batch format version 2 and the spots format for heal.
- Batch recipes (`.pfxbatch`) are at version 2 (presets.md section 4): the user's looks the
  stack refers to are embedded once each, a look on its own is a look step, a named size of the
  Export sheet can be the resize, and the watermark travels whole, its logo as base64. Opening
  a recipe (Batch page or `pixelfx --batch`) adds its looks to the user's looks when their id is
  new and its watermark unless an identical one is there (`core::adoptBatchResources`). Version 1
  files read unchanged, version 3 is refused as newer; library tests for both, app tests for the
  embedding, the adoption (twice, nothing doubled), a broken watermark and the page's round trip.
- The Batch page's Resize lists the Export sheet's named sizes, as `batch.html` shows ("Web ·
  2048 long edge"). This also fixes a bug: a size preset last used in Export silently decided
  the batch's size whatever Resize showed.
- Checked by hand: a version 2 recipe with a look and a centred text watermark, run with
  `pixelfx --batch` in a scratch profile, wrote a 1080 x 1080 square with the mark and added the
  look and the watermark. 32 app suites and 107 library tests pass.

### 2026-11-05 (mocks approved: grading, About, What's new)

- The owner approved `editor-grading.html`, `about.html` and `whats-new.html`.
- Color grading (E8) in the adjust panel, after the Color mixer: `ColorWheel` for the shadows
  and the highlights (angle the hue clockwise from red at the top, distance the amount, the
  values written under each wheel with the amount in the accent when set), the rim and the
  two-edged puck of the review, keys (Left and Right the hue, Up and Down the amount, Shift for
  10, Backspace resets; a held key is one history step), screen-reader text, Balance, the group
  dot and reset. `PIXELFX_DEBUG_EXPAND` opens a group for grabs.
- About (S11): the version and build, What's new, Source code and Report a bug, the credit
  line; Open-source notices with every shipped component and its licence text, copied verbatim
  from each project into `app/src/resources/notices` and built in (a test keeps the list in
  step with THIRD_PARTY.md); Diagnostics with versions (libpixelfx gained `engineVersions`),
  the system, the formats this build reads and writes, threads, Copy and Open the log folder
  (a test checks it holds no home folder, user name or licensing words).
- What's new (S8): the running version's section of the bundled changelog, the first five
  items and then "The full list of changes", a Version list once there are two releases, and
  "Show this after each update". It opens once after an update (500 ms after the window, never
  with `--batch`), and from the main menu and About. The changelog's lead is now two short
  paragraphs and its first five items are the mock's highlights.
- 31 app suites and 105 library tests pass; clang-tidy clean on the sources.

### 2026-11-05 (M3, part 1)

- Live editing: `RenderScheduler` renders previews on a worker, one at a time, with only the
  newest request waiting; `Document` keeps the decoded source, the settings and their history,
  with `setSettings` (live), `commit` (a step), `apply` (a preset as one step), `undo`, `redo`;
  `core::History` holds named settings snapshots (200 steps).
- The adjust panel from `editor.html`: Light, Color and Detail groups built from the registry,
  changed values in the accent, group dots and reset, double-click resets a slider, Reset all
  (Ctrl+R), meaningful tracks for temperature, tint and saturation. Undo, Redo and hold Space
  (or the eye button) for the original in the header.
- Verified: 12 suites pass against the snap runtime (new: history, adjust panel, document
  editing including 30 quick changes rendering fewer times with the last one winning); clang-tidy
  clean; a grab with the example preset applied matches the mock.
- Found and fixed: opening rendered the photo twice; changed values were not in the accent.
- CI: Linux and macOS green after the fixes; Windows still building its vcpkg packages.
- Export: `ExportSheet` from `export.html` (formats the build can write, quality, 16 bits for
  PNG and TIFF, original size or a long edge, metadata on or off, folder, file-name tokens,
  if-exists policy), `Exporter` (full resolution on a worker), a toast with the result,
  Ctrl+Shift+E; choices remembered in settings; `core::exportTarget` never picks the original by
  accident. libpixelfx gained `fitLongEdge` (Lanczos 3 in linear light) and `writableExtensions`.
  15 app suites and 43 library tests pass; clang-tidy clean.
- Histogram at the top of the adjust panel, computed with each render on the worker; 16 suites pass.
- Edits remembered per photo (`core::SidecarStore`): a recipe file per photo in the app's data
  folder, named by a fingerprint of the file (size and the first and last 64 KiB), read on the
  open worker, written in order on a one-thread pool after a short pause, flushed on close and
  before the next photo opens; no settings, no file. The library's recipe documents carry the
  `source` block of `presets.md`. 17 app suites and 44 library tests pass.
- Curves group (`CurveEditor`): RGB, R, G, B; click to add, drag, double-click to remove; drawn
  with the library's own sampler (`pfx::sampleCurve`) so the drawing is what renders.
- History popup (H) that jumps to any step; before and after split (\\) with a draggable divider.
- Auto (`pfx::autoAdjust`): exposure toward middle grey, blacks and whites stretched, a little
  contrast and vibrance when needed, a half-strength grey-world white balance for strong casts;
  the Auto button keeps curves and detail; `pfx apply --auto`. 48 library tests pass.
- CI Windows: vcpkg and Qt install; the build compiles with MSVC after one fix (an iterator
  spelled as a pointer). 13 of 17 suites passed on the first Windows run; the end-to-end
  single-instance test passed there. The four failures were test assumptions, now fixed: Unix
  paths in the command line and export tests, a /proc path in the log sink test, and a
  single-instance test sending from the primary's own thread (on Windows a local socket is a
  named pipe, which needs the other side to accept).
- Geometry in libpixelfx (`geometry` op, stage 10): quarter turns, flips, straightening that
  crops to the largest upright rectangle (2 px inside, so no soft corner shows), crop as
  fractions of the straightened photo; `pfx::outputSize` gives the size without rendering. A
  positive angle turns the photo clockwise (checked on a render). 53 library tests pass.
- Crop mode from `crop.html` (C): a rail with Edit and Crop; the crop box over the whole
  straightened photo with the rest dimmed, corner handles, Thirds, Grid, Golden and Diagonal
  guides; ratio chips (Free, Original, 1:1, 4:5, 5:7, 3:2, 16:9, 9:16), Landscape and Portrait
  (X), straighten slider in tenths of a degree, turn and flip buttons; Enter applies as one
  history step, Escape cancels. `core::turn` and `core::flip` act on the photo as shown (a turn
  under one flip goes the other way in the settings; a flip negates the angle and mirrors the
  crop). While cropping the preview renders without the crop, and moving the box renders
  nothing. Before and after now uses an original with the same geometry, so the halves line up.
  Not yet: the straighten dial under the photo, the level line, Auto straighten, Custom ratio
  and Perspective (the library has no perspective op yet). 19 app suites pass.
- Effects (vignette, grain, fade), Color mixer (eight bands) and colour grading (split toning)
  in the library; Effects and Color mixer groups in the panel from `editor.html`. Colour
  grading has no approved panel yet: `mocks/editor-grading.html` and its review are waiting for
  the owner (two questions in the review: a midtones wheel, own group or inside Color).
- The app opens what the libvips build reads (`pfx::readableExtensions`); dev builds now build
  libheif with libde265 and dav1d, libjxl 0.12 and LibRaw 0.22 (ADR-008 update), so HEIC,
  AVIF, JPEG XL and camera RAW open in dev runs. The welcome page shows Recent photos with
  their remembered edits.
- Speed: a 24 MP JPEG with a typical edit (light, white balance, curves, clarity, sharpen,
  vignette) loads, renders and saves in 1.1 s, down from 3.4 s (M3 target 1.5 s, met on a
  14-thread machine); peak memory 0.98 GB, down from 1.33 GB. Measured with `pfx apply` and
  per-step timing. Bounded memory (tiled rendering) is still open.
- CI: Windows (MSVC, vcpkg) green for the first time; macOS failed on a test that used
  QTest::mouseMove (it moves the real cursor there), fixed.
- Dehaze (dark channel prior, guided filter, haze map worked out at about 1000 px and applied
  at full size: 0.75 s and 0.69 GB at 24 MP instead of 2.35 s and 2.1 GB) in the library and
  the Detail group.
- Fuzzing (`make fuzz`): libFuzzer with ASan and UBSan. The document parser ran 8.5 million
  inputs clean after two fixes it found (a curve clamped into colliding points was kept broken;
  empty or dotted names and value-group collisions did not read back the same). The loader
  target runs about 2 inputs a second (thread start-up under the sanitizers), 600 runs clean;
  reading it showed an unbounded allocation from a hostile header, now refused over 512 MP.
- Zoom and pan from the zoom bar of `editor.html`: wheel zoom around the cursor, drag to pan,
  double-click for Fit and 100%, Ctrl+0, Ctrl+1, Ctrl+Plus and Ctrl+Minus, full screen; Crop
  mode always fits. Past the preview's resolution the Document decodes the full photo once and
  renders it 250 ms after edits settle (`setFullResolution`, `fullReady`); it is dropped when
  the view goes back. E switches to Edit mode. FEATURES has no row for zoom yet.
- Crop mode completed against `crop.html`: the straighten dial under the photo, Draw a level
  line (horizontal or upright, whichever is nearer), the Custom ratio chip, and the Perspective
  sliders (keystone in the library that never shows empty corners; turns and flips carry it).
  The four-corner fix and Auto straighten are left for later, as the mock allows.
- Color group: the White balance menu (As shot, Auto, Custom) and the picker (W), from
  `pfx::neutralWhiteBalance`, which inverts the white balance model. The mock's Kelvin presets
  (Daylight, Cloudy ...) are left out: on a JPEG they would be guesses; they come with RAW.
- Copy and paste settings (Ctrl+Shift+C, Ctrl+Shift+V) as a preset document on the clipboard,
  crop and turns excepted; paste a picture from the clipboard (Ctrl+V).
- Export: Keep location (off by default) strips GPS from Exif (unlinked and zeroed) and XMP
  without a metadata library; only formats with a working encoder are offered (AVIF and HEIC
  were offered and failed when libheif had no encoder).
- Settings sheet (`settings.html`, General page): theme (applied at start, since icons are
  tinted when made), canvas background, remember edits, on open, clipping warnings (J). The
  other pages and rows (monitor profile, GPU, batch threads, sidecars next to the photo) come
  with their features. The light theme was checked on a grab; the histogram now multiplies its
  channels on a light well.
- Presets (M4 started early, mocks `editor-presets.html` and `preset-save.html`): the left panel
  with the Presets tab (Looks waits for looks), grouped rows with pictures of the photo, search,
  import, the row menu, hover preview through `Document::setHoverSettings`; Save as preset with
  the units of the mock (White balance and Perspective apart from Color and Crop). A preset
  writes every value of each unit it holds, defaults included, so applying replaces exactly
  those units and presets stack. Built-in presets are a first draft made from today's
  operations (owner to tune).
- CI: Windows ran the full suite for the first time: two library tests assumed loaders the
  vcpkg libvips lacks (GIF, PPM); fixed in the tests. macOS: the curve test now double-clicks
  where the point is drawn.
- Lightroom and Camera Raw presets: `pfx::importXmpPreset` maps the crs: settings PixelFx has
  (the table of presets.md section 6) and lists the others; the Presets panel imports `.xmp` as
  "Lightroom: name" and says what was left out. Tried on hand-made presets only: real exports
  from Lightroom are worth a try.
- Filmstrip from the strip of `editor.html` and `apply-to-selected.html`: many photos open at
  once, pictures of their edits with the edited badge, the + tile, "1 of 4" or "N selected ·
  Select all · Clear", Left and Right (in the window's key handling so a focused slider keeps its
  arrows), Ctrl and Shift click, Ctrl+A. With a selection, presets and pasted settings go to every
  selected photo: the current one as a history step, the others through their remembered edits
  (needs Remember edits on), and the Presets panel says "Applies to N photos". Export with a
  selection is "Export N photos": the same sheet, each photo with its own settings, one after
  another. Paste with a selection asks which parts (Paste to N photos).
- Open: the curve editor's mouse test crashes (SIGSEGV) on the macOS CI runner only, after the
  synthetic double click; it is clean under ASan and UBSan on Linux (debug build, `build/asan`)
  and passes on Windows. Skipped on macOS for now; worth a look on a Mac.
- Batch (M5 started, mocks `batch.html`, `batch-running.html`, `batch-done.html`): the Batch
  mode on the rail (B) with the photo list (files, folders with subfolders, drops), the recipe
  (two presets stacked, Auto enhance first, keep edits already made), resize, format and quality,
  folder or a PixelFx folder beside each original, Process N photos; `services::BatchRunner` runs
  two photos at a time on its own pool with Pause, Stop, Retry failed, and each failed row says
  why in plain words. libpixelfx now explains a failed read in plain words instead of libvips's
  developer message. Batch recipes (`.pfxbatch`, presets.md section 4) are read and written by
  the library; the page saves and opens them, and `pixelfx --batch recipe files...` runs one
  without a window (tested end to end on the built binary). The title follows a running batch;
  quitting during one asks first. Rows have thumbnails and a remove button, and the preview row
  shows the first readable photo before and after the recipe. The welcome page has Open folder
  and Batch edit. Not yet: the watermark, the notification at the end.
- 2026-11-05: dev libvips reads HEIC, AVIF, JPEG XL and camera RAW (ADR-008 update):
  `build-deps.sh` builds libheif 1.23.7 (libde265 1.1.3, dav1d 1.5.4), libjxl 0.12.0 (highway
  1.4.0) and LibRaw 0.22.3 against the SDK overlay, about 2.5 minutes from scratch on 14 cores;
  a stamp of all pins rebuilds the prefix when one changes. Checked against the runtime:
  `heifload`, `jxlload`, `jxlsave` and `dcrawload` present; a host-made HEIC, 8 and 10-bit AVIF,
  a JPEG XL written by the dev libvips and a synthetic DNG all decode to the expected values.
  No AVIF or HEIC writing (no encoders). 19 app suites pass.
- Main menu from `menu.html` (F10): Open, Open folder, Open recent, Paste image; Edit, View and
  Photo submenus built from the window's own actions, so each shows the shortcut that works;
  Export, Import presets, Show presets folder, Settings, Keyboard shortcuts, Report a bug, Quit.
  Tab hides the side panels and the filmstrip (S12), only while the photo (a click on it) or
  nothing has the focus, so Tab still walks through sliders and fields. Left out until they
  exist: Save edit with the file, Copy to clipboard (Ctrl+Shift+C is Copy settings, so the mock's
  key clashes), What's new, Online guide, About. 26 app suites pass.
- Watermark (X10) from `watermark.html`: `core::Watermark` (text or logo, Auto, white, black or
  a picked colour, size and margin as shares of the long edge, nine anchors or a dragged
  position, repeat with an angle, soft shadow), drawn with QPainter onto a picture covering
  only the mark, then laid on the exported pixels in linear light by the new
  `pfx::blendOverlay`; Auto reads `pfx::meanLuminance` under the mark. Tokens {year}, {date},
  {camera} from the new `pfx::readExif` (fuzzed: 1.7 million runs clean). Saved watermarks are
  `.pfxwatermark` files (presets.md section 7, new) with the logo copied beside them. The sheet
  opens from a Watermark row in Export and in Batch (section 3, as `batch.html`); Batch no longer
  inherits Export's choice unseen. A watermark file that went missing fails the export instead
  of leaving the mark off. Preview on: This photo, Portrait, Square, and Batch photos when opened
  from Batch (each click decodes the next photo of the batch at preview size on a worker). Not
  yet: keeping the watermark in a `.pfxbatch` (needs batch format version 2, an owner decision). 27 app suites and 90 library
  tests pass.
- The exposure line of `editor.html` under the histogram ("ISO 400 · f/2.8 · 1/250 s · 35 mm",
  `core::exposureLine`), the camera and lens as its tooltip; checked on a scratch JPEG with a
  hand-made Exif block. Photos without exposure data show nothing there.
- LUTs in libpixelfx (L5, ahead of the look step): `pfx::parseCube` (1D and 3D, domain,
  input range, refusing short or long tables with the line number), `pfx::lutFromHald`,
  `pfx::applyLut` (tetrahedral, looked up in sRGB primaries and encoding as display LUTs expect,
  a mix in linear light), `pfx apply --lut FILE[:MIX]`. 0.22 s more for a 33-point cube on a
  24 MP photo (14 threads). A `.cube` fuzzer ran 10.8 million inputs clean. 96 library tests pass.
- Looks in libpixelfx (M4 groundwork, no UI): `pfx::Look` (steps from the registry or `lut`,
  LUT resources) and `pfx::applyLook` with a strength in linear light; `RenderOptions::look`
  runs it after colour grading and before sharpening and the effects (presets.md section 5).
  `.pfxlook` documents read and write (presets.md section 3, now written down exactly as version
  1: params under the operation's group, LUTs as base64 half floats in the `srgb-gamma`
  domain, unknown steps kept and skipped, bad resources left out with a warning). The document
  fuzzer ran 2.2 million inputs with a look seed, clean. 102 library tests pass.
- Copy to clipboard (X8) in the Export sheet footer, as `export.html` has it: the same pixels as
  an export (size, watermark) rendered on a worker (`Exporter::startCopy`, sharing
  `prepareExport` with Export), nothing written to disk; shown for one photo only. 27 app suites
  pass.
- Looks in the app (M4, mock `editor.html`): `core::LookLibrary` with the mock's 13 built-in
  looks (Vivid to Pastel, values are a draft for the owner to tune) built from registry steps,
  the user's looks in `<AppData>/looks` (`.cube` and HaldCLUT import become a one-LUT look),
  `core::findLook` (thread-safe) and `core::renderPhoto`, which every render now goes through
  (editor, thumbnails, Export, Batch). The left panel has Looks and Presets tabs; the Looks tab
  has search, category chips, tiles of the photo under each look (Original first), the applied
  look outlined, and Look strength at its foot (live, one history step on release, keys step by
  one). Choosing a look is one history step on the photo and the selection. Differences from
  the mock, until their pieces exist: no Favorites chip, and "Import LUT..." where the mock has
  "New look" (the look editor, L4). Save as preset has the mock's Look row ("Golden hour 72"),
  so a preset can carry a look. Batch offers the looks in its first choice ("Preset or look", as
  `batch.html`), at each look's own strength; a saved batch recipe refers to a built-in look by
  id, but does not embed a user's look yet (presets.md asks for that, so the file runs on
  another machine). 29 app suites pass.
- The look editor (L4, L6, mock `filter-editor.html`): New look beside My looks, Edit look,
  Export look and Delete on the user's tiles. A step list (registry operations and LUTs, a
  checkbox to switch one off, drag or Move up and down, Duplicate, Remove), the selected step's
  sliders built from the registry (the tone curve with the curve editor), a live preview on the
  photo or a colour ramp rendered on a worker, the default strength, Export look... and Bake to
  .cube... (`pfx::bakeLook`, 33 points, names the steps left out). Saved looks are `.pfxlook`
  files in the looks folder; an edited look re-renders the photo. Differences from the mock:
  Preview on Portrait and Landscape wait for test photos PixelFx can ship; the category list has
  no "Mine" (the user's looks are Mine already). 29 app suites pass.
- A review of the night's changes (a second reader, then checked by hand) found seven bugs, all
  fixed with tests: a look file's LUT with an empty or endless domain, or non-numbers, now is
  left out (it made every pixel NaN); exported looks had no id and never listed; names with a
  leading dot saved hidden files; saving a watermark could replace another of a similar name
  and lose its logo on a failed copy; a preset's Look unit from a photo without a look did not
  take a look off; a look tile was deleted while its own menu was running; a repeated
  watermark of a sliver of a logo could ask for millions of copies. App suites and library
  tests also ran clean under ASan and UBSan (29 and 104).

### 2026-11-04 (M2)

- The app skeleton in `app/`: layered static libraries (core, services, ui, app), the root
  `CMakeLists.txt` building the library and the app together, Qt 6.11 hygiene definitions.
  From the kit: single instance, log sink, changelog reader, icons helper, CMake helpers, test
  helper, dev scripts. Written for PixelFx: settings facade with the one-time PixelFx 1 folder
  migration, command line (files, `--profile`, `--quit`, log file), `Application`, `Document`
  (open and render on a worker, newest request wins), the image bridge, design tokens and style,
  the canvas, the welcome page (from the approved `welcome.html`; Open folder, Batch edit and
  Recent come with their features), the main window header (from `editor.html`), drag and drop,
  debug hooks. libpixelfx gained `toSrgb8` for display.
- ADR-008: `scripts/build-deps.sh` builds libvips 8.18.7 against the KDE snap SDK, so dev builds
  and tests run on the snap's libraries; the runtime farm keeps the host's libstdc++.
- Verified: builds with `-Werror` against the SDK; 10 test suites pass under the snap runtime
  (the library's 34 cases, settings, CLI, single instance, log sink, changelog guard, image
  bridge, document, smoke, and an end-to-end test of the real binary: a second launch hands over
  its photo, `--quit` ends the first). Headless grabs of the welcome page and an opened photo.
- Found and fixed: the fit view used the preview size as the 100% limit; file sizes now in
  decimal units as the mocks show. A manual check of `--quit` was unreliable (it matched the
  wrong process); the end-to-end test replaced it.
- CI (`.github/workflows/ci.yml`, `vcpkg.json`): Linux with warnings as errors, Windows with MSVC
  and vcpkg, macOS with Homebrew. Not run yet; it runs on the next push.

### 2026-11-04 (publishing)

- The rewrite is published in github.com/keshavbhatt/PixelFx (PLAN D20, owner's intent: a clean
  history for Flathub): PixelFx 1 kept on the `legacy-v1` branch and the `v1-final` tag (at
  `568c142`), `main` replaced by the new history (force-push with lease), description updated.
  github.com/keshavbhatt/pixel got an archive note and is archived.
- Before publishing: business notes and keyword research moved to `DOCS/private/` (git-ignored);
  `LICENSING.md`, `REUSE.toml`, `LICENSE` and `LICENSES/` added (`reuse lint` passes); a fresh
  clone builds and passes all tests.

### 2026-11-04 (M1)

- libpixelfx 0.1.0 in `libs/pixelfx-core`: float RGBA image in linear Rec.2020, colour maths,
  settings with dotted keys, the operation registry (8 operations, 24 parameters), PixelFx
  documents (recipe and preset) with unknown keys kept, the pipeline, load and save through
  libvips with LittleCMS, and `pfx` (`ops`, `info`, `apply`). ADR-007 (nlohmann/json, doctest).
- Operations: exposure; tone (contrast, highlights, shadows, whites, blacks; v1 is a global
  curve); white balance (relative temperature and tint, Bradford); vibrance and saturation;
  curves (monotone, RGB and per channel); clarity; sharpening; noise reduction.
- Verified: 34 test cases (872 assertions, 40 with ctest) pass, including golden images per
  operation (looked at by eye), scale tests (half-size render matches the full-size render
  downscaled) and identical output at 1 and 7 threads; clean under address and
  undefined-behaviour sanitizers; `-Werror` with GCC 16; clang-tidy and clang-format clean;
  `pfx apply` with the example preset on a real photo.
- Bugs found by the tests and tools and fixed: libvips served a stale cached decode when a file
  was overwritten on disk (now `revalidate`, with a regression test); greyscale images lost
  their grey band through libvips's static `bandjoin` overload (found by clang-tidy, now with a
  test).
- Not met, carried into M3 (ROADMAP): a 20 MP photo takes 2.5 s and about 1.1 GB because M1
  renders the whole image at once; tiled rendering inside libvips and SIMD are the plan. Dehaze
  moved to M3. Highway is not linked yet.
- Turned off three style-only clang-tidy checks new in clang-tidy 23 (maths parentheses,
  designated initializers, trailing commas).

### 2026-11-04

- Owner decisions: monorepo with `libs/pixelfx-core` and `app/`; app GPL-3.0-or-later with the
  licensing module under its own licence (Model B); library permissive (Apache-2.0 or MIT,
  PLAN Q1); name PixelFx, id `com.ktechpit.pixelfx`, library libpixelfx; design may be
  completely new; apply the principles of github.com/dickwu/apple-design-skill to the design
  process.
- Wrote `reference/analysis-pixelfx-v1.md` (old app and Go tool audited file by file: the Go
  tool was a separate process per apply, not a library), `FEATURES.md` (every v1 feature has a
  row), `LESSONS.md`, `ROADMAP.md`, `PLAN.md` (OPEN questions Q1 to Q9), `DECISIONS.md`
  (ADR-000 to ADR-006), `DESIGN.md` (tokens, layout, the
  five-lens review method), `presets.md` (draft file formats), `CODING_STANDARDS.md` (adapted
  from Omnidict with a library section), `CLAUDE.md`.
- Mocks: 16 screens in `mocks/` generated as plain HTML with drawn sample photos (no third-party
  images), live sliders and look tiles. First design review (`mocks/reviews/editor.md`) found
  three contrast failures and two small hit targets, fixed in the tokens and the sheet.
- Verified: every mock rendered in headless Chrome at 1440 by 900; token contrast computed from
  the hex values (DESIGN section 5).
- Library research (`research/image-libraries.md`, about 90 cited sources, runtimes checked
  from real checkouts) folded into ADR-003 and PLAN section 5.6: own float operations inside a
  libvips-driven pipeline, lcms2, LibRaw, the modern codecs, no Exiv2, no GPL in the library.
  Found a KDE neon bug: `kf6-core24` ships libjxl 0.7 while its JXL plugin links 0.11.
- Owner asked which features sell; FEATURES gained spot heal (E20, 2.0), remove background and
  AI noise reduction (E22, 2.1), watermark (X10), size presets for where the photo is going
  (X11), convert and resize only (M8), batch rename (M9), Lightroom import as a headline (P5),
  and a proposal for the licensing module (private notes).
- Flathub keyword research (private notes, `DOCS/private/seo/`): store text decided (PLAN D22).
- Mocks for spot heal (`heal.html`, a Heal mode on the rail, R), watermark (`watermark.html`) and
  size presets (`export-sizes.html`); Export and Batch gained Size and Watermark rows. Reviews
  found a white watermark at 1.8:1 on a pale sky: added an Auto colour, now the default.
- Owner accepted the recommendations for Q1 to Q11: recorded as PLAN D13 to D22 and carried
  into FEATURES and ADR-003 (Apache-2.0 library, CPU first with GPU in 2.1, RAW in 2.0, the
  licensing decisions (private notes), vector icon, edits in the app library, Windows and macOS in CI from M2,
  reuse the PixelFx repository, store text).
- Owner answered the rest: a licensing question (D23, private notes), PixelBatch is folded into PixelFx with
  optimise-only parity first (D24, FEATURES M10 to M13), all mocks approved (D25).
- Owner: PixelBatch's interface is not good and its reliance on several command line tools
  called through the shell is unwanted. ADR-002 now names optimisation explicitly (linked
  libraries only), FEATURES M10 says capability only, LESSONS gained a row.
- Owner decided PLAN D26 (private notes). New mock
  `batch-compress.html` for that mode (sizes before and after, savings, 100% compare), reviewed
  in `mocks/reviews/batch-compress.md`; waiting for the owner.
- Nothing open in PLAN section 9.
