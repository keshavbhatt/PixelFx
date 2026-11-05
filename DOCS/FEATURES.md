# FEATURES: the scope contract

Decision legend: KEEP (in 2.0), KEEP 2.1 (the first update after 2.0), LATER (after that),
DROP (never), OWNER (waits for the owner).
Status legend: `todo`, `wip`, `done (class)`, `verified` (exercised live on the owner's desktop).

Sources: `reference/analysis-pixelfx-v1.md` (the old app, "v1" below), the rewrite kit's Red
FEATURES for the desktop-shell features that proved themselves, and the owner's brief.

The owner's brief (2026-11-04): rewrite PixelFx from scratch; quick, cross-platform (Linux,
Windows, macOS), powerful image retouching and manipulation; the image logic becomes a
reusable open-source core library that replaces the Go `pixel` tool; presets, filters, the
ability to create custom filters and presets, and to apply them to many images at once; the
same stack as the other ktechpit apps; Snap Store and Flathub first. The design may be
completely new (owner, 2026-11-04).

Vocabulary (also in `PLAN.md` section 4): an **adjustment** is one slider or control; a
**recipe** is every setting of one photo; a **look** (the user's "filter") is a stack of
steps with one strength slider; a **preset** is a saved part of a recipe, optionally with a
look; a **batch recipe** is a preset plus resize and save options.

## A. Shell (window, identity, desktop)

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| S1 | One main window: header bar, mode rail (Edit, Crop, Batch), side panels, filmstrip | toolbar + dock | KEEP | todo |
| S2 | Persist window geometry and state, panel widths, last mode | geometry only | KEEP | wip: geometry and state; panels have fixed widths; last mode todo |
| S3 | Single instance; a second launch forwards its files to the running window | refuses with a dialog | KEEP | done (`SingleInstance`, `Application`; end-to-end `tst_instance`) |
| S4 | Open files from the command line and the desktop file (`%F`), MIME types declared | argv ignored | KEEP | wip: command line done (`CliOptions`); desktop file and MIME types with packaging (M7) |
| S5 | Drag and drop of files and folders anywhere in the window; paste an image from the clipboard | none | KEEP | wip: dropping files and pasting an image (Ctrl+V, kept as PNG in the cache) done; folders with the filmstrip and Batch |
| S6 | Native notifications through the portal or freedesktop (batch finished) | none | KEEP | todo |
| S7 | Crash handler, log file, diagnostics copy, Report a bug sheet | none | KEEP | wip: log file done (`LogSink`); the rest M6 |
| S8 | What's new once per version from the bundled changelog, Version picker, link in About; Online guide | none | KEEP | done: What's new (once after an update, menu, About); Online guide waits for a URL |
| S9 | Keyboard shortcuts sheet (F1, Ctrl+/); every action shows its shortcut in its tooltip | 3 shortcuts | KEEP | wip: the shortcuts sheet from `shortcuts.html` (F1, Ctrl+/, searchable); tooltips name their shortcut where set |
| S10 | Theme: System, Dark (default), Light; neutral grey chrome | dark only | KEEP | done: System, Dark, Light in Settings, applied at start |
| S11 | About with version, links, open-source notices, diagnostics | wrong source link | KEEP | done: version, What's new, source, bug report, notices with licence texts, diagnostics |
| S12 | Full screen (F11) and a distraction-free view (Tab hides the panels) | none | KEEP | done: F11, Tab while the photo has the focus, both in the View menu |
| S13 | Translations | none | LATER | |
| S14 | Native macOS menu bar; header menu elsewhere | none | KEEP (macOS with M7) | todo |

## B. Opening photos

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| O1 | Open images (many at once), Open folder (with subfolders), Recent list | one file | KEEP | wip: Open images (first photo shown) done; Recent on the welcome page with thumbnails of the remembered edits done (`WelcomePage`, `services::Thumbnailer`); filmstrip and Open folder with Batch |
| O2 | Formats in: JPEG, PNG, WebP, TIFF (8/16/32 bit), BMP, GIF (first frame), AVIF, HEIC/HEIF, JPEG XL | png, jpg, bmp, tiff, webp (broken in snap) | KEEP | wip: the app opens whatever the libvips build reads (`pfx::readableExtensions`); HEIC, AVIF, JPEG XL in dev builds in progress |
| O3 | Camera RAW (DNG and the common makes) with a sensible default rendering | none | KEEP (LibRaw, PLAN D15) | todo |
| O4 | EXIF orientation applied on open | ignored | KEEP | done: the library applies the orientation tag on load (M1) |
| O5 | Embedded ICC profile honoured; working space is linear and wide gamut | dropped | KEEP | done for input: embedded profiles converted to linear Rec.2020 through lcms2 (M1); display profile todo |
| O6 | Decoding off the GUI thread, a fast embedded or downscaled preview first, full quality after | GUI thread, decoded twice | KEEP | wip: off the GUI thread at screen size done (`Document`); progressive refinement in M3 |
| O7 | Image information: size, format, bit depth, colour profile, camera, lens, exposure, date, GPS (behind a click) | Info dialog | KEEP (panel, not a dialog) | wip: size, format and file size in the header; the exposure line under the histogram (editor.html) with camera and lens as its tooltip, from `pfx::readExif`; the full panel todo (no mock yet) |
| O8 | Temp PNG copies of every opened file | yes | DROP | |

## C. Editing (adjustments)

All adjustments are non-destructive settings in the recipe, rendered by the library;
preview and export use resolution-independent parameters so they match (v1 defect 3).

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| E1 | Light: Exposure (stops), Contrast, Highlights, Shadows, Whites, Blacks | Brightness, Contrast, Sigmoidal contrast, Gamma | KEEP | wip: library done (tone v1 is a global curve; a local version may follow as v2); UI M3 |
| E2 | Color: White balance (As shot, Auto, presets, Temperature, Tint, picker), Vibrance, Saturation | Saturation, Color balance R/G/B | KEEP | wip: relative temperature and tint, vibrance, saturation, the As shot / Auto / Custom menu and the picker (W) done; Kelvin presets (Daylight, Cloudy ...) come with RAW, where the camera white balance is known |
| E3 | Color mixer: hue, saturation, luminance for 8 colour bands | Hue shift | KEEP | done: library (`mixer`, stage 80) and the Color mixer group of `editor.html` |
| E4 | Curves: RGB and per channel, points, double-click removes | none | KEEP | done (`CurveEditor`, `AdjustPanel`; drawn with `pfx::sampleCurve`) |
| E5 | Levels (black, mid, white point per channel) | none | LATER | |
| E6 | Detail: Clarity (local contrast), Sharpen (amount, radius, masking), Noise reduction (luminance, colour), Dehaze | Sharpen, Blur, Mean, Median | KEEP | done: clarity, sharpen, noise reduction and dehaze in the library and the Detail group |
| E7 | Effects: Vignette (amount, midpoint, roundness), Grain (amount, size), Fade | none | KEEP | done: library (vignette amount, midpoint, roundness; grain amount, size; fade) and the Effects group with the three sliders of `editor.html` |
| E8 | Split toning / color grading (shadows, midtones, highlights wheels) | Colorize | KEEP (as a step in looks and a Color group sub-panel) | done: shadows and highlights wheels and balance in a Color grading group (approved mock: no midtones), and as a look step |
| E9 | Creative operations from v1: Sepia, Threshold, Colorize, Pixelate, Blur, Invert, Grayscale, Emboss, Edge, Sobel | some in UI, some only in Go | KEEP as look steps, not sliders | todo |
| E10 | Auto: one click sets Light and Color from the image statistics | none | KEEP | done (`pfx::autoAdjust`; the Auto button and Ctrl+Shift+A; `pfx apply --auto`) |
| E11 | Live preview while dragging (no apply on release), 60 fps target on the preview size | apply on release, one process per apply | KEEP | wip: live while dragging, newest request first (`RenderScheduler`); speed work (tiles, SIMD) in M3 |
| E12 | Histogram (RGB, luminance), clipping warnings (J), camera line | RGB on GUI thread | KEEP | wip: RGB histogram on the render worker and clipping warnings (J, Settings) done; camera line todo |
| E13 | Before and after: split with divider, side by side, hold Space for the original | hold button | KEEP | wip: the split with a draggable divider (\\) and holding Space done (`Canvas`, `MainWindow`); side by side in M3 |
| E14 | Undo and redo for every change; History list; named snapshots | none | KEEP | done: undo, redo, the History popup (H) and named snapshots kept in the photo's recipe |
| E15 | Copy and paste settings between photos, choose groups | none | KEEP | wip: copy and paste of all settings but geometry, through the clipboard as a preset document (`core::settings_clipboard`); choosing groups with the filmstrip (M4) |
| E16 | Reset a slider (double-click), a group, or everything | per row, global | KEEP | done (`AdjustPanel`) |
| E17 | Edits remembered per photo in a sidecar (app library by default, or next to the photo) | none | KEEP | wip: app library by default, keyed by a file fingerprint so renames and moves keep the edit, written off the GUI thread (`core::SidecarStore`, `Document`); next to the photo and the setting in M3 |
| E18 | GPU-accelerated preview with automatic CPU fallback | none | KEEP 2.1 (PLAN D14) | |
| E19 | Local adjustments: brush, linear and radial gradient masks | none | LATER | |
| E20 | Spot heal: click or brush over a blemish, dust spot or small object; the patch is found automatically and blended; spots are stored in the recipe (relative coordinates), editable and removable | none | KEEP (sells: the app is a "retouching" app, owner 2026-11-04) | done: Heal mode (R) from `heal.html`, spots and brush strokes, patch picked, source draggable, size, feather, opacity, Show spots (V), Reveal dust (D), list, undo; stored as `heal.spots` (recipes version 2) on the upright photo |
| E20b | Clone brush, red eye removal | none | LATER | |
| E21 | Layers, text, drawing, compositing | none | DROP (not a raster editor; Krita and GIMP own that) | |
| E22 | Remove background (subject cut-out to transparent PNG or WebP, or onto a colour), on the user's machine, model downloaded once on first use | none | KEEP 2.1 (sells: product photos, online sellers; an ADR on the model and its licence first) | |
| E22b | AI noise reduction | none | KEEP 2.1 or LATER (after E22, same runtime) | |
| E22c | Subject and sky select for masks, upscaling | none | LATER (needs E19) | |

## D. Looks (filters), built in and custom

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| L1 | Looks panel: tiles render the current photo, categories, search, favourites | none | KEEP | wip: the Looks tab of `editor.html` (search, All, Mine, Color, Film, B&W chips, My looks and Built in as tiles of the photo with each look, Original first, keyboard choosable); Favorites todo |
| L2 | One strength slider per applied look (0 to 100), stored in the recipe | none | KEEP | done: Look strength at the foot of the Looks tab, live while dragging, one history step on release; `look.id` and `look.strength` in the photo's settings (presets.md section 2), rendered in the editor, thumbnails, Export and Batch through `core::renderPhoto` |
| L3 | About 30 built-in looks (colour, film, black and white, creative), made from library operations, shipped as files | none | KEEP | wip: the 13 of `editor.html` (Vivid to Pastel), made from registry steps in code (`core::builtInLooks`), values a draft for the owner; the rest, and shipping them as files, todo |
| L4 | Custom look editor: ordered steps from the operation registry, enable, reorder, duplicate, parameters, preview on test images | none | KEEP | wip: New look and Edit look from `filter-editor.html` (steps from the registry and LUTs, switch off, drag or Move up and down, duplicate, remove, sliders built from the registry, the tone curve, live preview on the photo or a colour ramp, default strength, Save look); preview on test photos todo (none shipped yet) |
| L5 | LUT steps: import `.cube` (1D and 3D) and HaldCLUT PNG | none | KEEP | wip: the library reads `.cube` (1D, 3D, DOMAIN_MIN and DOMAIN_MAX) and HaldCLUT pictures and applies them (tetrahedral, in the sRGB encoding, with a mix; `pfx apply --lut`), as `lut` steps of a look with the table embedded; the app todo |
| L6 | Export a look as `.pfxlook`; bake the colour-only steps to a `.cube` | none | KEEP | done: Export look and Bake to .cube in the look editor (33 points, steps that are not colour-only named as left out), Export look on a tile of My looks |
| L7 | Apply a look to the selection in the filmstrip | none | KEEP | done: a look chosen with several photos selected goes to each, as presets do (`apply-to-selected.html`) |

## E. Presets

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| P1 | Save the current settings as a preset, choosing which groups it contains | none | KEEP | done: Save as preset from `preset-save.html`, units ticked as the mock says, Save to file |
| P2 | Preset library with groups: My presets, Imported, Built in; search; hover preview | none | KEEP | wip: library with My presets, own groups, Imported, Built in; search; hover preview on the photo; pictures of the photo with each preset. Favourites todo |
| P3 | Preset row menu: apply to selected, rename, update with current, duplicate, export, show file, delete | none | KEEP | done: Apply, Rename, Update with current settings, Export, Show file, Delete (asks first); Apply to selected with the filmstrip |
| P4 | `.pfxpreset` JSON format, versioned, documented in `DOCS/presets.md`, readable by the library and the CLI | none | KEEP | done: `.pfxpreset` read and written through libpixelfx; presets hold whole units so they stack (`core::presets`) |
| P5 | Import Lightroom / Camera Raw `.xmp` presets: map the settings PixelFx has, report what was skipped. A headline feature: "use your Lightroom presets without a subscription" (owner, 2026-11-04) | none | KEEP | wip: `.xmp` import in the library (`pfx::importXmpPreset`) and the Presets panel, with the list of what was left out; checked on hand-made presets, real Lightroom exports to be tried |
| P6 | Import darktable styles | none | LATER | |
| P7 | Built-in presets including "Auto enhance" (per-image analysis) | none | KEEP | wip: Auto enhance, Bright and airy, Punchy, Soft matte from the operations there are; more with looks |

## F. Crop and geometry

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| G1 | Crop mode with handles, dimmed outside, ratio chips (Free, Original, 1:1, 4:5, 5:7, 3:2, 16:9, 9:16, Custom), orientation swap, overlays | dialog, shapes, destructive | KEEP (non-destructive, in place) | done: Crop mode with every ratio chip (Custom asks for numbers), orientation, overlays (`CropPanel`, `Canvas`, `core::crop_geometry`) |
| G2 | Straighten: dial, slider, drawn level line | none | KEEP | done: dial under the photo, slider in tenths of a degree, drawn level line (horizontal or upright); Auto straighten later (mock: LATER if unreliable) |
| G3 | Auto straighten (horizon detection) | none | LATER | |
| G4 | Rotate 90 left and right, flip horizontal and vertical | rotate right only, flips stuck | KEEP | done: turns and flips act on the photo as shown, crop included |
| G5 | Perspective: vertical, horizontal sliders and four-corner keystone | none | KEEP | wip: vertical and horizontal keystone in the library and Crop mode; four-corner fix todo |
| G6 | Lens corrections (distortion, chromatic aberration, vignetting by profile) | none | LATER | |
| G7 | Elliptical and circular crops with transparent corners | yes | KEEP as an Export option "Round crop" for PNG/WebP | todo |
| G8 | Resize as an edit (as opposed to on export) | none | DROP (resize lives in Export and Batch) | |

## G. Many photos at once

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| M1 | Filmstrip of every open photo; Ctrl and Shift selection; Ctrl+A | none | KEEP | done: filmstrip of every open photo with pictures of their edits, Left and Right, Ctrl and Shift click, Ctrl+A (`Filmstrip`) |
| M2 | With a selection, applying a look or preset, pasting settings, and Export act on all selected | none | KEEP | done: presets, pasted settings (asking which parts) and Export ("Export N photos", each with its own settings) act on the selection |
| M3 | Batch page: add files and folders, recipe (looks and presets stacked, or settings from a photo), optional Auto first, resize, format, folder, name tokens | none | KEEP | wip: Batch page from `batch.html` (photos and folders, two stacked presets, Auto first, keep edits, resize, format, folder or beside the originals, Process); before and after preview, watermark and saved batch recipes todo |
| M4 | Batch runs on all cores in the background with a memory cap; pause, stop, per-file status and reason, retry failed, notification | none | KEEP | wip: background runner, two photos at a time (memory), Pause, Stop, per-photo status and reason, Retry failed, Show in folder; notification todo |
| M5 | Save and reuse batch recipes (`.pfxbatch`) | none | KEEP | done: Save as batch recipe and Open recipe on the Batch page (`.pfxbatch` version 2: presets, the user's looks, the size preset and the watermark with its logo embedded, so it runs on another machine) |
| M6 | Command line batch: `pixelfx --batch recipe.pfxbatch files...` and the library's own `pfx` CLI | none | KEEP | done: `pixelfx --batch recipe.pfxbatch files...` without a window, a line per photo, exit code 1 when one failed; `pfx apply` for the library |
| M7 | Watch folder (process new files automatically) | none | LATER | |
| M8 | Convert and resize only: a Batch recipe with no edits, for jobs like HEIC to JPG or "shrink 300 photos for email"; one click from the welcome page | none | KEEP | todo |
| M9 | Rename in batch with tokens and a live preview of the first names | none | KEEP | todo |
| M10 | Optimise only (PixelBatch parity, PLAN D24; capability only, nothing from PixelBatch's interface or code, and no external tools: every step through linked libraries, ADR-002): shrink files without visible change. JPEG lossless (Huffman optimisation, progressive, strip metadata, through libjpeg-turbo's transform API) or to a target quality; PNG lossless (filter and deflate search with libpng and zopfli, Apache-2.0) or lossy palette quantisation (libimagequant-compatible, through libvips with the MIT `quantizr`, never the GPL libimagequant in the library); WebP and AVIF re-encode; GIF through cgif | PixelBatch: jpegoptim, pngquant, gifsicle | KEEP | todo |
| M11 | Per-file results: original size, new size, saving in bytes and percent, totals; re-process with other settings; a before-and-after compare of any row at 100% (mock `batch-compress.html`) | PixelBatch | KEEP | todo |
| M12 | Concurrent task limit, output folder or in place (asks once), name prefix or suffix, remember last folders | PixelBatch | KEEP (part of M3, M4) | todo |
| M13 | SVG optimisation | PixelBatch (SVGO, Node) | DROP: not a photo format, would need a Node runtime; named in PixelBatch's farewell note | |

## H. Export

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| X1 | Export sheet: JPEG, PNG, WebP, AVIF, TIFF, JPEG XL, HEIC (where the platform allows); per-format options with correct semantics | Qt writers, inverted PNG slider | KEEP | wip: sheet with the formats the build writes, quality, 16 bits (`ExportSheet`, `Exporter`); AVIF, HEIC, JPEG XL with the codec parts |
| X2 | Resize on export: long edge, width, height, megapixels, percent; never enlarge | none | KEEP | wip: long edge, never enlarging (`pfx::fitLongEdge`); the other modes with size presets (X11) |
| X3 | Output sharpening for screen or print | none | KEEP | todo |
| X4 | Colour space: sRGB (default), Display P3, Adobe RGB, same as original; ICC embedded | dropped | KEEP | todo |
| X5 | Metadata: keep camera details and copyright (default on), keep location (default off), strip all | dropped | KEEP | wip: keep or strip all done; removing only the location needs Exif editing (M3) |
| X6 | Folder, name tokens ({name}, {n}, {date}, {preset}, {w}, {h}), if-exists policy (number, overwrite with one confirmation, skip) | no overwrite check | KEEP | wip: folder, {name} {date} {w} {h}, add a number, overwrite, skip, never the original by accident (`core::exportTarget`); {n} and {preset} with batch, the overwrite confirmation with the sheets |
| X7 | Estimated file size; options remembered | none | KEEP | wip: options remembered (`Settings::exportOptions`); the estimate later in M3 |
| X8 | Copy the result to the clipboard; share through the portal | none | KEEP (clipboard); LATER (share) | done (clipboard): Copy to clipboard in the Export sheet (`export.html`) copies the exported pixels, size and watermark included, rendered at full resolution on a worker |
| X9 | Export is off the GUI thread with progress; failures say why | use-after-free, silent | KEEP | wip: off the GUI thread, saved or the reason in a toast; a progress bar for large photos in M3 |
| X10 | Watermark on export and in batch: text (font, colour, opacity) or a logo image (PNG with alpha), nine anchor positions plus margin, size relative to the image, optional tiling; saved watermarks reused by name; never applied to the editor view | none | KEEP | wip: the sheet of `watermark.html` from Export and Batch (text with {year}, {camera}, {date} or a logo, Auto colour, size, opacity, margin, nine positions or dragged, repeat with an angle, soft shadow, preview on the photo, portrait and square), saved as `.pfxwatermark` (presets.md section 7), laid in linear light on the final pixels, Preview on the batch's own photos from Batch; carried inside batch recipes (version 2) |
| X11 | Size presets for where the photo is going: Instagram post (1080 by 1350) and square (1080), story (1080 by 1920), Facebook, X, web (2048 long edge), email (1600), 4K wallpaper, print sizes at 300 dpi; crop ratio follows when the photo is cropped for it; user-defined sizes | none | KEEP | wip: the presets of `export-sizes.html` in the Export sheet with Crop to fill (centred) or Fit inside (white border); dragging the frame, Custom size and "Mine" todo, and Batch (needs a batch format version 2) |

## I. The core library (libpixelfx)

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| C1 | Standalone C++20 library, no Qt, own CMake project in `libs/pixelfx-core`, Apache-2.0 (PLAN D13) | Go CLI, MIT | KEEP | done (libpixelfx 0.1.0) |
| C2 | Float32 linear-light pipeline, tiled, resolution-independent parameters, deterministic output | 8-bit sRGB | KEEP | wip: float linear Rec.2020, relative radii, deterministic across thread counts done (`pfx::render`); tiled rendering inside libvips is M3 |
| C3 | Operation registry with typed parameter schemas (id, range, default, unit, version); drives the UI, presets and look steps | flags | KEEP | done (`pfx::Registry`) |
| C4 | Recipe, preset, look and batch formats with JSON (de)serialisation and migration by version | none | KEEP | wip: recipe and preset done (`pfx::Document`), unknown keys kept, newer versions refused; looks M4, batch M5; the first migration comes with format version 2 |
| C5 | Codecs and metadata behind interfaces; dependencies per ADR-003 | Go stdlib png/jpeg | KEEP | wip: load and save through libvips with lcms2, orientation, Exif/XMP/IPTC kept (`pfx::load`, `pfx::save`); RAW, HEIC, AVIF, JPEG XL verified in M3 |
| C6 | Batch engine (thread pool, memory budget, cancellation, progress callbacks) | none | KEEP | todo |
| C7 | `pfx` command line tool (apply preset/look/recipe, convert, resize) | `pixel` | KEEP | wip: `pfx ops`, `pfx info`, `pfx apply` (presets, `--set`, format, quality, bits, long edge) done; batch recipes M5, LUT baking M4 |
| C8 | Golden-image tests per operation with tolerances; fuzzed decoders and preset parsers | none | KEEP | wip: golden and scale tests per operation done; fuzzing M3 |
| C9 | Python bindings | none | LATER | |
| C10 | Public repository split (git subtree) | separate repo | LATER (owner decides when) | |

## J. Settings surface

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| T1 | Settings sheet with sidebar (General, Editor, Export, Batch, Presets and looks, Performance, Advanced), writes straight to settings | one checkbox | KEEP | todo |
| T2 | Native or Qt file dialogs | yes | DROP (portal dialogs always; the kit's rule) | |
| T3 | Canvas background (dark grey, middle grey, black, white) | none | KEEP | todo |
| T4 | Display colour management (monitor profile) | none | KEEP | todo |
| T5 | Sidecar location (app library by default, next to the photo as an option, PLAN D18) and "on open" behaviour | none | KEEP | todo |
| T6 | Batch thread count, preview backend | none | KEEP | todo |

## K. Accounts and licensing (never in user-facing release text)

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| K1 | Shared AccountAndLicense module under its own licence (Model B), path reserved | none | KEEP (module) | todo |
| K2 | What the module gates: decided by the owner, recorded in the private notes (`DOCS/private/business.md`) | none | KEEP | |
| K3 | Old users: no account existed in v1, nothing to migrate | none | DROP | |
