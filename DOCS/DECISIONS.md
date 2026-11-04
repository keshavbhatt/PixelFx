# Architecture Decision Records

Short, numbered, append-only. Format: Context, Decision, Consequences. Headings use a colon,
not a dash. ADRs inherited from Red 10 (the rewrite kit) are listed once in ADR-000.

---

## ADR-000: inherited from the rewrite kit (2026-11-04)

PixelFx adopts these decisions from Red 10 and Omnidict (numbers refer to
`rewrite-kit/reference/red-docs/DECISIONS.md`) without discussion:

- Qt 6.11 minimum built against the `kde-qt6-core24-sdk` snap SDK, run against the
  `kf6-core24` runtime; C++20 and CMake only; warnings as errors in CI; layered static
  libraries; no `.ui` files; PMF connects only; `QT_NO_KEYWORDS`, `QT_NO_CAST_FROM_ASCII`,
  `QT_USE_QSTRINGBUILDER`.
- One typed `core::Settings` facade; every key once in `settings_keys.h`.
- Portals first: file chooser, notifications, OpenURI and file manager; freedesktop D-Bus as
  fallback.
- Single instance over `QLocalServer` with JSON commands; `--profile` for isolated profiles.
- Crash handler, rotating log, diagnostics, Report a bug, What's new from the bundled
  changelog with a Version picker, About with diagnostics.
- Headless verification hooks documented, not hidden; licence overrides only in Debug builds
  (R-ADR-010).
- Licensing through the shared AccountAndLicense module with a per-app config; nothing about it
  in user-facing release text (R-ADR-011), under its own licence (kit Model B).
- Every dialog is the app's own sheet.
- Snap built by GitHub Actions; Flathub manifest extracts the published snap.

Not inherited: everything web (WebEngine, scripts, cookies, user agents) and the download
engine; PixelFx has neither.

## ADR-001: monorepo with a Qt-free library and a Qt app (2026-11-04)

**Context.** v1 was two repositories: the Qt app and a Go command line tool wrapping
`disintegration/gift`, joined by a submodule and a shell command. The owner wants the image
logic to become a reusable open-source library and the app rewritten on the kit's stack.

**Decision.** One repository. `libs/pixelfx-core` is libpixelfx: a standalone CMake project,
C++20, no Qt, permissive licence, namespace `pfx`, with its own tests and `pfx` CLI. `app/`
consumes it with `add_subdirectory` and is GPL-3.0-or-later (kit Model B). The library can be
mirrored to its own public repository with `git subtree split` later (FEATURES C10).

**Consequences.** The library's public API is designed for outside users from the start
(versioned, documented, no app assumptions). A GPL dependency can never enter the library; the
app may use GPL components that the library does not need. CI builds and tests the library
alone on three platforms.

## ADR-002: in-process image processing (2026-11-04)

**Context.** v1 ran `bash -c "./pixel <flags> file.png"` on every slider release and read the
encoded image from stdout; failures were silent, latency was seconds, and nothing could be
cancelled (analysis section 3.1, LESSONS row 1).

**Decision.** All image work runs in-process through libpixelfx: decoding, rendering,
analysis, encoding, and file optimisation. No helper process, no shell, and no external
command line tool for image work. This covers the optimisation PixelFx takes over from
PixelBatch (PLAN D24), which shelled out to jpegoptim, pngquant, gifsicle and SVGO and parsed
their output: PixelFx does the same work through linked libraries (libjpeg-turbo's transform
API, libpng with zopfli, a libimagequant-compatible quantiser through libvips, cgif).

**Consequences.** A crash in a codec is a crash of the app, so decoders are fuzzed (CODING
STANDARDS section 11) and the crash handler records the file being decoded. Batch work runs on
threads with a memory budget, not in processes.

## ADR-003: library dependencies (2026-11-04)

**Context.** `research/image-libraries.md` evaluated libvips, OpenCV, ImageMagick,
GraphicsMagick, GEGL, OpenImageIO, Halide, Skia, CImg, stb, LittleCMS2, Exiv2, libexif, LibRaw,
the modern codecs, darktable and RawTherapee code, and GPU paths, against the library's
permissive licence (ADR-001), float linear processing (ADR-004), the snap and Flathub runtimes,
and Windows and macOS. No library provides the retouching operations with float linear-light
semantics, so the adjustment maths is ours; the question is what carries it. The owner
suggested libvips is powerful enough to lean on.

**Decision.**
- **Own operations** in C++20 on float32 linear Rec.2020 tiles, vectorised with Google Highway
  (Apache-2.0 / BSD-3).
- **libvips** (LGPL-2.1-or-later, linked dynamically, reduced build) is the I/O and execution
  backbone: decode and encode through its loaders, shrink-on-load for fast first pixels and
  thumbnails, Lanczos3 resampling, affine and `mapim` warps for geometry, demand-driven
  tile evaluation with bounded memory for export and batch (`sink_disc`), and the async tile
  renderer for the interactive preview (`sink_screen`). Our operations run inside its pipeline
  through `vips_image_generate`. The public API exposes only `pfx` types; libvips stays a
  private dependency, so it could be replaced by our own scheduler if the GPU path (PLAN D14) ever
  needs one (the research's alternative).
- **LittleCMS2** (MIT) for ICC: input profiles to the working space and the output transform,
  float on tiles. Its `fast_float` and threaded plugins are GPL-3 and are not used.
- **LibRaw** (LGPL-2.1 or CDDL-1.0, dynamic) for RAW in 2.0 (PLAN D15), through libvips's loader
  or directly for camera white balance and matrices.
- **Codecs** through libvips's modules: libjpeg-turbo, libpng, libtiff, libwebp, libheif with
  libde265 (HEIC decode) and dav1d or aom (AVIF), libavif, libjxl; OpenEXR optional. No x265
  (GPL-2): HEIC export is off until a permissive HEVC encoder is acceptable; AVIF and JPEG XL
  cover modern export.
- **Metadata in the library**: Exif, XMP and ICC blobs travel through libvips (libexif,
  LGPL) with the copy, strip and copy-without-GPS policies; HEIF, AVIF and JPEG XL get their
  blobs through the codec box APIs. **Exiv2 is not used** (GPL-2.0-or-later, and its BMFF
  writing is missing anyway). If M1 shows a gap the blob path cannot close, Exiv2 enters only
  as a separate GPL component of the app behind a `MetadataStore` interface, never in the
  library.
- **Rejected as the engine**: GEGL (GObject, the useful operations are GPL-3), OpenCV
  (whole-image, no ICC; possible later as an optional dependency for inpainting), ImageMagick and
  GraphicsMagick (whole-image, small maintainer base), OpenImageIO and OpenColorIO (VFX
  weight), Halide (LLVM toolchain; a later experiment for kernels), Skia (a compositor). darktable
  and RawTherapee are GPL and not libraries: algorithm references only.
- **GPU** (2.1, PLAN D14): in the app through Qt RHI (`QRhiWidget`, fragment or compute
  passes for the pointwise operations); the CPU path stays authoritative for export and golden
  tests.

**Consequences.**
- Every dependency is permissive or LGPL and dynamically linked, so the library is
  Apache-2.0 (PLAN D13) and the app GPL.
- Packaging (research section 7): the Flathub KDE 6.11 runtime has the codecs, lcms2 and
  libexif but no libvips or LibRaw, so those are manifest modules; HEIC decode needs the
  `codecs-extra` extension. In the snap, libvips, libjxl 0.12 and LibRaw 0.22 are built as parts
  (the `kf6-core24` content snap ships libjxl 0.7 while its JXL image plugin links 0.11, so JXL
  there is broken: report to KDE neon), libde265 and its libheif plugin are staged with
  `LIBHEIF_PLUGIN_PATH`. Windows and macOS use vcpkg ports for all of them.
- Exact runtime contents are in `research/runtime-contents.md`. Two facts from it: the
  `kf6-core24` content snap already ships LibRaw 0.21.2, which the snap may use until a camera
  needs 0.22; and KDE 6.12 on freedesktop 26.08 (in beta) brings libjxl 0.12, libheif 1.23 and
  lcms2 2.19, so moving the Flatpak to it removes most of the version gap with the snap parts.
- A smoke test in the built snap and Flatpak opens one file of every supported format
  (ROADMAP M7).
- libvips's threading is the library's: `vips_concurrency_set` and `vips_cache_set_max_mem` are
  set from the batch budget and the editor's needs (ADR-006).

## ADR-004: the editing model (2026-11-04)

**Context.** v1 re-ran a parameter string on an 8-bit downscaled PNG for preview and on the
original for export, in an order fixed by the Go tool, with pixel-unit parameters; crop was
destructive and there was no undo.

**Decision.**
- An edit is a recipe of settings (`DOCS/presets.md` section 2). The pipeline order is fixed
  and documented (`presets.md` section 5); identity settings are skipped.
- Processing is float32 in linear light in a wide-gamut working space; 8 and 16 bit exist only
  at the codec boundary.
- Spatial parameters are fractions of the image; the renderer converts them for the scale it
  renders at. A scale test per operation enforces it.
- Geometry (crop, straighten, perspective, 90 degree turns, flips) is part of the recipe and is
  applied as one resampling at the start of the pipeline.
- Undo, history and snapshots store setting deltas, never pixels.

**Consequences.** Undo is instant and cheap; presets and batch reuse the same data; preview
matches export. Operations must be written for float and tiles; v1's gift-based semantics are
re-derived, not ported (the gift parameter meanings in the analysis inform the creative looks).

## ADR-005: the operation registry and the file formats (2026-11-04)

**Context.** v1 spelled each parameter in three places (UI, flag string, Go flags) with
different ranges, which produced its mapping bugs (LESSONS row 5). The owner wants presets,
custom filters and batch.

**Decision.** Every operation registers an `OpSpec` once in the library: id, group, parameter
specs (type, range, default, unit, relative flag), version and migration, `pointwise` flag. The
app builds its adjustment panel and look-step editor from the registry; preset, look, recipe and
batch files (one JSON envelope, `presets.md`) store values in registry units; `pfx ops` prints
the registry. Format changes bump the version and add a migration with a test in the same
commit.

**Consequences.** Adding an operation is one registration plus its tests; the UI and the file
formats follow automatically. Display strings for operations live in the app (translatable),
keyed by operation id.

## ADR-006: threads (2026-11-04)

**Context.** v1 decoded, encoded and computed histograms on the GUI thread.

**Decision.** The GUI thread only paints and handles input. One `RenderScheduler` worker owns
the interactive pipeline and coalesces requests (only the newest settings render; stale
generations are dropped). libvips's worker threads render tiles (ADR-003). Thumbnails run at low
priority. Batch runs about half the cores' worth of photos at once, each with its share of threads, leaving
one core for the editor, within the memory budget of PLAN section 5.4.

**Consequences.** Every service result reaches the UI as a queued signal with a generation
number. Tests drive the scheduler with a synchronous executor.

## ADR-007: JSON and test libraries for libpixelfx (2026-11-04)

**Context.** The library reads and writes the JSON documents of `presets.md` and needs a test
framework that does not depend on Qt (Qt Test would pull Qt into a Qt-free library).

**Decision.** nlohmann/json 3.12.0 (MIT) for documents, used only inside `src/` so no JSON type
appears in the public headers; doctest 2.4.12 (MIT) for the library's tests. Both come from the
system when a new enough version is installed, otherwise CMake fetches the release archive and
checks its SHA-256. The app keeps Qt Test.

**Consequences.** A first configure without the system packages needs the network once. Swapping
the JSON library later touches only `src/recipe/document.cpp`.
