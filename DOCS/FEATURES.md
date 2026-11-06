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
| S1 | One main window: header bar, mode rail (Edit, Crop, Batch), side panels, filmstrip | toolbar + dock | KEEP | done: `MainWindow` with the header, the rail (Edit, Crop, Heal, Batch, Settings at its foot), the side panels and the `Filmstrip` |
| S2 | Persist window geometry and state, panel widths, last mode | geometry only | KEEP | wip: geometry and state; panels have fixed widths; last mode todo |
| S3 | Single instance; a second launch forwards its files to the running window | refuses with a dialog | KEEP | done (`SingleInstance`, `Application`; end-to-end `tst_instance`) |
| S4 | Open files from the command line and the desktop file (`%F`), MIME types declared | argv ignored | KEEP | wip: command line done (`CliOptions`); desktop file and MIME types with packaging (M7) |
| S5 | Drag and drop of files and folders anywhere in the window; paste an image from the clipboard | none | KEEP | done: files dropped anywhere; a folder (with subfolders) dropped anywhere or chosen with Open folder goes to Batch; presets, looks and .cube LUTs dropped are imported; a file PixelFx does not open is said in a toast (2026-11-06); Paste image (Ctrl+V) takes an image or photos copied in a file manager, kept as PNG in the cache |
| S6 | Native notifications through the portal or freedesktop (batch finished) | none | KEEP | done on Linux: a batch that ends while PixelFx is in the background notifies through the portal (Flatpak) or org.freedesktop.Notifications, a click brings the window to Batch (ADR-011, 2026-11-06); Windows and macOS with M7 |
| S7 | Crash handler, log file, diagnostics copy, Report a bug sheet | none | KEEP | wip: log file (`LogSink`); diagnostics with Copy and Open the log folder in About (2026-11-05); Report a bug opens the issue tracker (main menu, About); the crash handler and a Report a bug sheet todo (M6) |
| S8 | What's new once per version from the bundled changelog, Version picker, link in About; Online guide | none | KEEP | done: What's new (once after an update, menu, About); Online guide in the main menu and About, to the GitHub wiki (`core::guideUrl`, 2026-11-06) |
| S9 | Keyboard shortcuts sheet (F1, Ctrl+/); every action shows its shortcut in its tooltip | 3 shortcuts | KEEP | wip: the shortcuts sheet from `shortcuts.html` (F1, Ctrl+/, searchable); tooltips name their shortcut where set |
| S10 | Theme: System, Dark (default), Light; neutral grey chrome | dark only | KEEP | done: System, Dark, Light in Settings, switched at once, and System follows the desktop while PixelFx runs (2026-11-06) |
| S11 | About with version, links, open-source notices, diagnostics | wrong source link | KEEP | done: version, What's new, source, bug report, notices with licence texts, diagnostics |
| S12 | Full screen (F11) and a distraction-free view (Tab hides the panels) | none | KEEP | done: F11, Tab while the photo has the focus, both in the View menu |
| S13 | Translations | none | LATER | |
| S14 | Native macOS menu bar; header menu elsewhere | none | KEEP (macOS with M7) | todo |

## B. Opening photos

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| O1 | Open images (many at once), Open folder (with subfolders), Recent list | one file | KEEP | done: Open images fills the filmstrip (`Filmstrip`); Open folder (Ctrl+Shift+O, welcome page) puts a folder's photos, subfolders included, on the Batch page; Recent on the welcome page with thumbnails of the remembered edits (`WelcomePage`, `services::Thumbnailer`) and Open recent in the main menu |
| O2 | Formats in: JPEG, PNG, WebP, TIFF (8/16/32 bit), BMP, GIF (first frame), AVIF, HEIC/HEIF, JPEG XL | png, jpg, bmp, tiff, webp (broken in snap) | KEEP | wip: the app opens whatever the libvips build reads (`pfx::readableExtensions`); dev builds read HEIC, AVIF, JPEG XL and camera RAW (libheif, libjxl and LibRaw in `scripts/build-deps.sh`, checked 2026-11-05); the snap and Flatpak builds with M7 |
| O3 | Camera RAW (DNG and the common makes) with a sensible default rendering | none | KEEP (LibRaw, PLAN D15) | wip: `editor-raw.html` built (2026-11-06): LibRaw through libvips, linear DNG (JPEG XL or uncompressed, phones) read by libpixelfx with the DNG default rendering (ADR-012), the Camera profile, the embedded JPEG first and for thumbnails, Open its preview instead, Kelvin and light sources for DNG; lens profiles: G6 |
| O4 | EXIF orientation applied on open | ignored | KEEP | done: the library applies the orientation tag on load (M1) |
| O5 | Embedded ICC profile honoured; working space is linear and wide gamut | dropped | KEEP | done for input: embedded profiles converted to linear Rec.2020 through lcms2 (M1); display profile todo |
| O6 | Decoding off the GUI thread, a fast embedded or downscaled preview first, full quality after | GUI thread, decoded twice | KEEP | done: decoded on a worker at screen size (`Document`), a RAW's embedded JPEG shown while it develops (2026-11-06), the full-size picture read once zoomed past the preview (`setFullResolution`, `editor-zoom-loading.html`) |
| O7 | Image information: size, format, bit depth, colour profile, camera, lens, exposure, date, GPS (behind a click) | Info dialog | KEEP (panel, not a dialog) | wip: size, format and file size in the header; the exposure line under the histogram (editor.html) with camera and lens as its tooltip, from `pfx::readExif`; the full panel drawn in `photo-details.html` (2026-11-06), waiting for the owner |
| O8 | Temp PNG copies of every opened file | yes | DROP | |

## C. Editing (adjustments)

All adjustments are non-destructive settings in the recipe, rendered by the library;
preview and export use resolution-independent parameters so they match (v1 defect 3).

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| E1 | Light: Exposure (stops), Contrast, Highlights, Shadows, Whites, Blacks | Brightness, Contrast, Sigmoidal contrast, Gamma | KEEP | done: library (tone v1 is a global curve; a local version may follow as v2) and the Light group of `editor.html` (`AdjustPanel`) |
| E2 | Color: White balance (As shot, Auto, presets, Temperature, Tint, picker), Vibrance, Saturation | Saturation, Color balance R/G/B | KEEP | done: relative temperature and tint, vibrance, saturation, the As shot / Auto / Custom menu and the picker (W); for RAW, Temperature in Kelvin and the light sources Daylight to Flash (`editor-raw.html`, 2026-11-06; Kelvin for RAW only, owner) |
| E3 | Color mixer: hue, saturation, luminance for 8 colour bands | Hue shift | KEEP | done: library (`mixer`, stage 80) and the Color mixer group of `editor.html`; picking a colour on the photo (T) moves its bands by their shares, in the B&W mix too (`editor-mixer-target.html`, 2026-11-05) |
| E4 | Curves: RGB and per channel, points, double-click removes | none | KEEP | done (`CurveEditor`, `AdjustPanel`; drawn with `pfx::sampleCurve`) |
| E5 | Levels (black, mid, white point per channel) | none | LATER | |
| E6 | Detail: Clarity (local contrast), Sharpen (amount, radius, masking), Noise reduction (luminance, colour), Dehaze | Sharpen, Blur, Mean, Median | KEEP | done: clarity, sharpen, noise reduction and dehaze in the library and the Detail group |
| E7 | Effects: Vignette (amount, midpoint, roundness), Grain (amount, size), Fade | none | KEEP | done: library (vignette amount, midpoint, roundness; grain amount, size; fade) and the Effects group with the three sliders of `editor.html` |
| E8 | Split toning / color grading (shadows, midtones, highlights wheels) | Colorize | KEEP (as a step in looks and a Color group sub-panel) | done: shadows and highlights wheels and balance in a Color grading group (approved mock: no midtones), and as a look step; reopened for P8 (owner, 2026-11-05): midtones and global, luminance per range and blending, done with `editor-develop.html` |
| E9 | Creative operations from v1: Sepia, Threshold, Colorize, Pixelate, Blur, Invert, Grayscale, Emboss, Edge, Sobel | some in UI, some only in Go | KEEP as look steps, not sliders | done (2026-11-06): Sepia, Colorize, Threshold, Invert, Blur, Pixelate, Emboss and Edges as `creative` steps in the look editor's Add step; Grayscale is the Black and white step, Edge and Sobel are one Edges step |
| E10 | Auto: one click sets Light and Color from the image statistics | none | KEEP | done (`pfx::autoAdjust`; the Auto button and Ctrl+Shift+A; `pfx apply --auto`) |
| E11 | Live preview while dragging (no apply on release), 60 fps target on the preview size | apply on release, one process per apply | KEEP | wip: live while dragging, newest request first (`RenderScheduler`); speed work (tiles, SIMD) in M3 |
| E12 | Histogram (RGB, luminance), clipping warnings (J), camera line | RGB on GUI thread | KEEP | wip: RGB histogram on the render worker, clipping warnings (J, Settings) and the camera line under it (`core::exposureLine`, camera and lens as its tooltip) done; a luminance histogram todo |
| E13 | Before and after: split with divider, side by side, hold Space for the original | hold button | KEEP | done: the split with a draggable divider (\\), Side by side from the View menu, holding Space for the original (`Canvas`, `MainWindow`, 2026-11-05) |
| E14 | Undo and redo for every change; History list; named snapshots | none | KEEP | done: undo, redo, the History popup (H) and named snapshots kept in the photo's recipe |
| E15 | Copy and paste settings between photos, choose groups | none | KEEP | done: copy and paste of all settings but geometry, through the clipboard as a preset document (`core::settings_clipboard`); pasting to several selected photos asks which parts (`PasteSheet`, `apply-to-selected.html`) |
| E16 | Reset a slider (double-click), a group, or everything | per row, global | KEEP | done (`AdjustPanel`) |
| E17 | Edits remembered per photo in a sidecar (app library by default, or next to the photo) | none | KEEP | wip: app library by default, keyed by a file fingerprint so renames and moves keep the edit, written off the GUI thread (`core::SidecarStore`, `Document`); Remember edits in Settings; next to the photo later (owner, 2026-11-06) |
| E17b | Zoom: Fit (Ctrl+0), 100% (Ctrl+1), in and out (Ctrl+= and Ctrl+-, the wheel), pan by dragging; the sharp full-size picture loads around the view with a busy ring | Zoom in and out | KEEP | done (`Canvas`, the zoom bar of `editor.html`; `editor-zoom-loading.html`) |
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
| L1 | Looks panel: tiles render the current photo, categories, search, favourites | none | KEEP | wip: the Looks tab of `editor.html` (search, All, Mine, Color, Film, B&W chips, My looks and Built in as tiles of the photo with each look, Original first, keyboard choosable); Favorites done (2026-11-06: F or the tile menu stars a look, the Favorites chip lists them, remembered) |
| L2 | One strength slider per applied look (0 to 100), stored in the recipe | none | KEEP | done: Look strength at the foot of the Looks tab, live while dragging, one history step on release; `look.id` and `look.strength` in the photo's settings (presets.md section 2), rendered in the editor, thumbnails, Export and Batch through `core::renderPhoto` |
| L3 | About 30 built-in looks (colour, film, black and white, creative), made from library operations, shipped as files | none | KEEP | wip: the 13 of `editor.html` (Vivid to Pastel), made from registry steps in code (`core::builtInLooks`), values a draft for the owner; the rest, and shipping them as files, todo |
| L4 | Custom look editor: ordered steps from the operation registry, enable, reorder, duplicate, parameters, preview on test images | none | KEEP | wip: New look and Edit look from `filter-editor.html` (steps from the registry and LUTs, switch off, drag or Move up and down, duplicate, remove, sliders built from the registry, the tone curve, live preview on the photo or a colour ramp, default strength, Save look); preview on test photos todo (none shipped yet) |
| L5 | LUT steps: import `.cube` (1D and 3D) and HaldCLUT PNG | none | KEEP | done: the library reads `.cube` (1D, 3D, DOMAIN_MIN and DOMAIN_MAX) and HaldCLUT pictures and applies them (tetrahedral, in the sRGB encoding, with a mix; `pfx apply --lut`), as `lut` steps of a look with the table embedded; the app imports a `.cube` or HaldCLUT as a one-LUT look (`core::LookLibrary`) and adds LUT steps in the look editor |
| L6 | Export a look as `.pfxlook`; bake the colour-only steps to a `.cube` | none | KEEP | done: Export look and Bake to .cube in the look editor (33 points, steps that are not colour-only named as left out), Export look on a tile of My looks |
| L7 | Apply a look to the selection in the filmstrip | none | KEEP | done: a look chosen with several photos selected goes to each, as presets do (`apply-to-selected.html`) |

## E. Presets

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| P1 | Save the current settings as a preset, choosing which groups it contains | none | KEEP | done: Save as preset from `preset-save.html`, units ticked as the mock says, Save to file |
| P2 | Preset library with groups: My presets, Imported, Built in; search; hover preview | none | KEEP | done: library with My presets, own groups, Imported, Built in; search; hover preview on the photo; pictures of the photo with each preset; Favorites (F, the tile's menu) and Recent (`editor-presets-catalog.html`, 2026-11-05) |
| P3 | Preset row menu: apply to selected, rename, update with current, duplicate, export, show file, delete | none | KEEP | done: Apply, Rename, Update with current settings, Export, Show file, Delete (asks first); Apply to selected with the filmstrip |
| P4 | `.pfxpreset` JSON format, versioned, documented in `DOCS/presets.md`, readable by the library and the CLI | none | KEEP | done: `.pfxpreset` read and written through libpixelfx; presets hold whole units so they stack (`core::presets`) |
| P5 | Import Lightroom / Camera Raw `.xmp` presets: map the settings PixelFx has, report what was skipped. A headline feature: "use your Lightroom presets without a subscription" (owner, 2026-11-04) | none | KEEP | wip: `.xmp` import in the library (`pfx::importXmpPreset`, `pfx import`) and the Presets panel, with the list of what was left out; 449 of EmulsionPresets' 451 import with nothing skipped (2026-11-05); a second set of real Lightroom exports still to try (P8's 95% check) |
| P8 | Lightroom compatibility (owner, 2026-11-05): every global develop setting a Lightroom or Camera Raw preset uses is read, rendered by the library and shown as a control: B&W treatment and mix, full colour grading (shadows, midtones, highlights, global, luminance, blending, balance), Texture, grain roughness, Calibration, vignette feather and style, sharpen detail, noise reduction detail and smoothness, parametric curve splits, CA removal, defringe. Same kind of result, not pixel-identical. Out of scope: masks and local tools, AI tools, lens blur, point colour; lens profiles came with ADR-013. Secondary sliders sit under a "More" disclosure in their group. Target: 95% or more of real presets import with nothing skipped | none | KEEP | done (2026-11-05) but the 95% check on a second set: library and importer (449 of EmulsionPresets' 451 import with nothing skipped, the other 2 use a lens profile, mapped since ADR-013), the panel of `editor-develop.html` (Color or Black & white with V, the B&W mix, Profile, four-range grading, curve Regions, Texture, More per group, Optics, Calibration), and Black & white, Optics and Calibration as preset parts |
| P9 | A large built-in preset catalog with categories (film, B&W, cinematic, portrait, seasons, places ...), browsed as tiles of the photo, with an Amount slider that blends from the photo's settings before the preset to the preset's own; curated and renamed from EmulsionPresets (MIT), no trademark or real person's name | none | KEEP | done (2026-11-05): 431 presets in 12 categories plus Essentials (`editor-presets-catalog.html`), Favorites, Recent, Amount 0 to 200; names approved by the owner, Tools kept last |
| P6 | Import darktable styles | none | LATER | |
| P7 | Built-in presets including "Auto enhance" (per-image analysis) | none | KEEP | done: Essentials with Auto enhance (per photo), Bright and airy, Punchy, High key B&W, each with a line saying what it does, and the catalog of P9; the Essentials' values a draft for the owner |

## F. Crop and geometry

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| G1 | Crop mode with handles, dimmed outside, ratio chips (Free, Original, 1:1, 4:5, 5:7, 3:2, 16:9, 9:16, Custom), orientation swap, overlays | dialog, shapes, destructive | KEEP (non-destructive, in place) | done: Crop mode with every ratio chip (Custom asks for numbers), orientation, overlays (`CropPanel`, `Canvas`, `core::crop_geometry`) |
| G2 | Straighten: dial, slider, drawn level line | none | KEEP | done: dial under the photo, slider in tenths of a degree, drawn level line (horizontal or upright); Auto straighten later (mock: LATER if unreliable) |
| G3 | Auto straighten (horizon detection) | none | LATER | |
| G4 | Rotate 90 left and right, flip horizontal and vertical | rotate right only, flips stuck | KEEP | done: turns and flips act on the photo as shown, crop included |
| G5 | Perspective: vertical, horizontal sliders and four-corner keystone | none | KEEP | wip: vertical and horizontal keystone in the library and Crop mode; four-corner fix todo |
| G6 | Lens corrections (distortion, chromatic aberration, vignetting by profile) | none | KEEP (owner, 2026-11-06; ADR-013) | done (2026-11-06): Lensfun, `lens_profile`, the photo's lens from Exif, the Lightroom keys imported, presets version 4, and Use lens profile in Optics (`editor-lens-profile.html`) naming the lens found, with Distortion and Vignetting; packages add the Lensfun database with M7 |
| G7 | Elliptical and circular crops with transparent corners | yes | KEEP as an Export option "Round crop" for PNG/WebP | todo |
| G8 | Resize as an edit (as opposed to on export) | none | DROP (resize lives in Export and Batch) | |

## G. Many photos at once

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| M1 | Filmstrip of every open photo; Ctrl and Shift selection; Ctrl+A | none | KEEP | done: filmstrip of every open photo with pictures of their edits, Left and Right, Ctrl and Shift click, Ctrl+A (`Filmstrip`); Close photo (owner, 2026-11-06, `close-photo.html`): Ctrl+W and the main menu, a × and a menu on each tile, the next photo or the welcome page after, a question first when Remember edits is off and the edit would be lost |
| M2 | With a selection, applying a look or preset, pasting settings, and Export act on all selected | none | KEEP | done: presets, pasted settings (asking which parts) and Export ("Export N photos", each with its own settings) act on the selection |
| M3 | Batch page: add files and folders, recipe (looks and presets stacked, or settings from a photo), optional Auto first, resize, format, folder, name tokens | none | KEEP | done: Batch page from `batch.html` (photos and folders, two stacked presets or looks or settings from a photo, Auto first, keep edits, resize with the named sizes, format, folder or beside the originals, name tokens, the watermark, the first photo before and after, saved batch recipes, Process) |
| M4 | Batch runs on all cores in the background with a memory cap; pause, stop, per-file status and reason, retry failed, notification | none | KEEP | done on Linux: background runner, two photos at a time by default for memory (Photos at a time in Settings), Pause, Stop, per-photo status and reason, Retry failed, Show in folder; a notification when a batch ends in the background (S6, ADR-011, 2026-11-06); Windows and macOS notifications with M7 |
| M5 | Save and reuse batch recipes (`.pfxbatch`) | none | KEEP | done: Save as batch recipe and Open recipe on the Batch page (`.pfxbatch` version 2: presets, the user's looks, the size preset and the watermark with its logo embedded, so it runs on another machine) |
| M6 | Command line batch: `pixelfx --batch recipe.pfxbatch files...` and the library's own `pfx` CLI | none | KEEP | done: `pixelfx --batch recipe.pfxbatch files...` without a window, a line per photo, exit code 1 when one failed; `pfx apply` for the library |
| M7 | Watch folder (process new files automatically) | none | LATER | |
| M8 | Convert and resize only: a Batch recipe with no edits, for jobs like HEIC to JPG or "shrink 300 photos for email"; one click from the welcome page | none | KEEP | done: Convert or resize on the welcome page opens Batch with "Convert and resize only" (no edits); Make files smaller opens Compress only (2026-11-06) |
| M9 | Rename in batch with tokens and a live preview of the first names | none | KEEP | done: the name field of both Batch modes takes {name}, {n}, {taken}, {date}, {camera}, {w}x{h}, {preset} and text, a + Token menu with examples, the first three names as they will be saved, and a warning when every photo would get the same name (2026-11-06) |
| M10 | Optimise only (PixelBatch parity, PLAN D24; capability only, nothing from PixelBatch's interface or code, and no external tools: every step through linked libraries, ADR-002, ADR-010): shrink files without visible change. JPEG lossless (Huffman optimisation, progressive, through libjpeg-turbo's coefficient API) or to a target quality; PNG lossless (smallest colour type and bit depth, filters, deflate) or fewer colours (our own palette quantiser with a quality floor, ADR-010); GIF, TIFF and lossless WebP stored again without loss; location and embedded previews removed on request | PixelBatch: jpegoptim, pngquant, gifsicle | KEEP | done: `pfx::compress` and `pfx compress` in the library; Compress only on the Batch page from `batch-compress.html` (2026-11-06). zopfli for slightly smaller PNGs is left for later |
| M11 | Per-file results: original size, new size, saving in bytes and percent, totals; re-process with other settings; a before-and-after compare of any row at 100% (mock `batch-compress.html`) | PixelBatch | KEEP | done: each row's sizes, bar and saving, the totals, Run again with these settings (it replaces what the last run wrote), the before and after of the selected row at 100% on its most detailed spot (2026-11-06) |
| M12 | Concurrent task limit, output folder or in place (asks once), name prefix or suffix, remember last folders | PixelBatch | KEEP (part of M3, M4) | done: Photos at a time in Settings, Performance (Automatic, 1, 2, 4, 8); a folder (remembered) or the originals (Compress only, asks each run); prefix and suffix as text around tokens in the name (2026-11-06) |
| M13 | SVG optimisation | PixelBatch (SVGO, Node) | DROP: not a photo format, would need a Node runtime; named in PixelBatch's farewell note | |

## H. Export

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| X1 | Export sheet: JPEG, PNG, WebP, AVIF, TIFF, JPEG XL, HEIC (where the platform allows); per-format options with correct semantics | Qt writers, inverted PNG slider | KEEP | wip: sheet with the formats the build writes, the rarer ones under More, quality, 16 bits (`ExportSheet`, `Exporter`); JPEG XL written in dev builds; AVIF and HEIC wait for an encoder in the build |
| X2 | Resize on export: long edge, width, height, megapixels, percent; never enlarge | none | KEEP | wip: long edge, never enlarging (`pfx::fitLongEdge`); width, height or both through the size presets and Custom size (X11, 2026-11-06); megapixels and percent todo |
| X3 | Output sharpening for screen or print | none | KEEP | done (`pfx::sharpenForOutput`, after resizing; Export) |
| X4 | Colour space: sRGB (default), Display P3, Adobe RGB, same as original; ICC embedded | dropped | KEEP | done (`SaveOptions::space`; Export; Batch stays sRGB) |
| X5 | Metadata: keep camera details and copyright (default on), keep location (default off), strip all | dropped | KEEP | done: keep camera details and copyright (on by default) or strip all; Keep location (off by default) removes GPS from Exif and XMP (`pfx::withoutLocation`), in Export and Batch |
| X6 | Folder, name tokens ({name}, {n}, {date}, {preset}, {w}, {h}), if-exists policy (number, overwrite with one confirmation, skip) | no overwrite check | KEEP | wip: folder, {name} {n} {date} {taken} {camera} {preset} {w} {h}, add a number, overwrite, skip, never the original by accident (`core::exportTarget`, `core::NameFacts`, 2026-11-06); the overwrite confirmation todo |
| X7 | Estimated file size; options remembered | none | KEEP | done (the preview encoded at up to 1 megapixel, carried to the output size) |
| X8 | Copy the result to the clipboard; share through the portal | none | KEEP (clipboard); LATER (share) | done (clipboard): Copy to clipboard in the Export sheet (`export.html`) copies the exported pixels, size and watermark included, rendered at full resolution on a worker |
| X9 | Export is off the GUI thread with progress; failures say why | use-after-free, silent | KEEP | wip: off the GUI thread, a toast with the photo being exported (Exporting 2 of 5), then saved or the reason; a progress bar for large photos todo |
| X10 | Watermark on export and in batch: text (font, colour, opacity) or a logo image (PNG with alpha), nine anchor positions plus margin, size relative to the image, optional tiling; saved watermarks reused by name; never applied to the editor view | none | KEEP | done: the sheet of `watermark.html` from Export and Batch (text with {year}, {camera}, {date} or a logo, Auto colour, size, opacity, margin, nine positions or dragged, repeat with an angle, soft shadow, preview on the photo, portrait and square), saved as `.pfxwatermark` (presets.md section 7), laid in linear light on the final pixels, Preview on the batch's own photos from Batch; carried inside batch recipes (version 2); the mock's font weight left for later |
| X11 | Size presets for where the photo is going: Instagram post (1080 by 1350) and square (1080), story (1080 by 1920), Facebook, X, web (2048 long edge), email (1600), 4K wallpaper, print sizes at 300 dpi; crop ratio follows when the photo is cropped for it; user-defined sizes | none | KEEP | done: the grouped list of `export-sizes.html` in Export and Batch, the framing card with a draggable frame, Crop to fill or Fit inside (black or white border by the photo), Custom size with width, height, dpi and Mine, 300 dpi for prints (2026-11-06); Mine in Batch waits for a batch recipe version |

## I. The core library (libpixelfx)

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| C1 | Standalone C++20 library, no Qt, own CMake project in `libs/pixelfx-core`, Apache-2.0 (PLAN D13) | Go CLI, MIT | KEEP | done (libpixelfx 0.1.0) |
| C2 | Float32 linear-light pipeline, tiled, resolution-independent parameters, deterministic output | 8-bit sRGB | KEEP | wip: float linear Rec.2020, relative radii, deterministic across thread counts done (`pfx::render`); tiled rendering inside libvips is M3 |
| C3 | Operation registry with typed parameter schemas (id, range, default, unit, version); drives the UI, presets and look steps | flags | KEEP | done (`pfx::Registry`) |
| C4 | Recipe, preset, look and batch formats with JSON (de)serialisation and migration by version | none | KEEP | done: recipe, preset, look and batch documents (`pfx::Document`), unknown keys kept, newer versions refused; migration by version, each with a test (presets version 4, recipes and batch recipes version 5) |
| C5 | Codecs and metadata behind interfaces; dependencies per ADR-003 | Go stdlib png/jpeg | KEEP | done: load and save through libvips with lcms2, orientation, Exif/XMP/IPTC kept (`pfx::load`, `pfx::save`); HEIC, AVIF, JPEG XL and camera RAW checked in dev builds (2026-11-05); linear DNG read by libpixelfx (ADR-012) |
| C6 | Batch engine (thread pool, memory budget, cancellation, progress callbacks) | none | KEEP | wip: the engine lives in the app for now (`services::BatchRunner`: its own pool, photos at a time, pause, stop, per-photo progress and reasons; the Batch page and `pixelfx --batch`); `pfx::BatchRunner` in the library with a memory budget todo |
| C7 | `pfx` command line tool (apply preset/look/recipe, convert, resize) | `pixel` | KEEP | wip: `pfx ops`, `pfx info`, `pfx apply` (presets and recipes, `--set`, `--auto`, `--lut`, format, quality, bits, long edge), `pfx import` (Lightroom `.xmp`), `pfx compress`, `pfx apply --look` and `pfx bake` (a look to `.cube`, 2026-11-06) done; batch recipes from the command line are the app's (`pixelfx --batch`, the watermark is drawn with Qt) |
| C8 | Golden-image tests per operation with tolerances; fuzzed decoders and preset parsers | none | KEEP | done: golden and scale tests per operation; fuzzing with `make fuzz` (libFuzzer, ASan and UBSan): the loader, documents, `.cube` and Exif (2026-11-05) |
| C9 | Python bindings | none | LATER | |
| C10 | Public repository split (git subtree) | separate repo | LATER (owner decides when) | |

## J. Settings surface

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| T1 | Settings sheet with sidebar (General, Editor, Export, Batch, Presets and looks, Performance, Advanced), writes straight to settings | one checkbox | KEEP | wip: the sheet of `settings.html` (`SettingsSheet`), writing straight to settings, with one General page (Appearance, Editing, Display, Performance); the other pages todo |
| T2 | Native or Qt file dialogs | yes | DROP (portal dialogs always; the kit's rule) | |
| T3 | Canvas background (dark grey, middle grey, black, white) | none | KEEP | done: Dark gray, Middle gray, Black, White in Settings, applied at once (`Canvas`) |
| T4 | Display colour management (monitor profile) | none | KEEP | todo |
| T5 | Sidecar location (app library by default, next to the photo as an option, PLAN D18) and "on open" behaviour | none | KEEP | wip: Remember edits (the app's library) and On open (as it was left, or the original) in Settings; next to the photo later (owner, 2026-11-06) |
| T6 | Batch thread count, preview backend | none | KEEP | wip: Photos at a time (Automatic, 1, 2, 4, 8) in Settings (2026-11-06); the preview backend with the GPU preview (E18, 2.1) |

## K. Accounts and licensing (never in user-facing release text)

| # | Feature | v1 | Decision | Status / class |
|---|---|---|---|---|
| K1 | Shared AccountAndLicense module under its own licence (Model B), path reserved | none | KEEP (module) | wip: path reserved (`app/src/modules/AccountAndLicense`, `LICENSING.md`); the module todo |
| K2 | What the module gates: decided by the owner, recorded in the private notes (`DOCS/private/business.md`) | none | KEEP | |
| K3 | Old users: no account existed in v1, nothing to migrate | none | DROP | |
