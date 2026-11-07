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

## ADR-008: dev builds get libvips from the SDK, and keep the host C++ runtime (2026-11-04)

**Context.** The kit builds against the `kde-qt6-core24-sdk` snap and runs against the
`kf6-core24` runtime so that what compiles here is what ships (ADR-000). libvips is in neither
snap. Taking it from the host would mix Arch's glib and codecs with Ubuntu's at run time. The
SDK has every library libvips needs, but some of its development links (`libglib-2.0.so`,
`libz.so`, `libpng16.so`, `libexpat.so`) point into the runtime snap or the core24 base and
dangle off-snap, and its `lcms2.pc` names two plugin libraries the SDK does not contain. The
kit's runtime farm also put core24's libstdc++ (GCC 14) ahead of the host's, which code from the
host's GCC 16 cannot run with.

**Decision.** `scripts/build-deps.sh` builds the pinned libvips (8.18.7, SHA-256 checked) with
meson against an overlay of the SDK: the dangling links resolved into the runtime snap first,
then core24; the phantom lcms2 plugins dropped; the SDK's bare include directory moved after the
host's libc headers. It enables only the formats the app needs. `dev-build.sh` calls it and builds
the app and the library against the same overlay; `dev-run.sh` runs with libvips from
`build/deps/prefix` and the runtime snap. The runtime farm now excludes libstdc++ and libgcc_s
with the glibc family: the host compiler's runtime stays authoritative.

**Consequences.** Dev builds and tests use the snap's glib, libjpeg, libpng, libtiff, libwebp,
lcms2 and libexif. HEIF, JPEG XL and RAW have no development files in the SDK; they arrive with
M3 as snap parts and the same overlay. The snap itself builds libvips as a part from the same
pinned source on core24 (M7). CI on Ubuntu, Windows (vcpkg) and macOS (Homebrew) uses each
platform's own libvips (PLAN D19).

**Update (2026-11-05).** `build-deps.sh` now also builds, pinned and SHA-256 checked, the codecs
the SDK has no development files for, so dev libvips reads HEIC, AVIF, JPEG XL and camera RAW:
libheif 1.23.7 with libde265 1.1.3 (HEVC) and dav1d 1.5.4 (AV1) built in, no plugin loading
and no encoders (no x265, ADR-003; no AV1 encoder, so AVIF is read-only in dev builds); libjxl
0.12.0 (decoder and encoder) with highway 1.4.0 and the SDK's brotli and lcms2; LibRaw 0.22.3
with the SDK's libjpeg, zlib and lcms2, no OpenMP. highway, dav1d and libde265 are static and
end up inside libjxl and libheif, so the runtime's older `libhwy.so.1` and `libdav1d.so.7` are
not shadowed; the prefix's `libheif.so.1` (1.23.7) does shadow the runtime's 1.17.6, which is
ABI compatible. The CMake projects search only the prefix, the overlay and the SDK
(`CMAKE_FIND_ROOT_PATH`), never the host's `/usr`. dav1d uses its x86 assembly only when `nasm`
is on the host (a build tool); without it the C paths decode the same pixels, more slowly. A
stamp of all pinned versions in the prefix triggers a clean rebuild when any pin changes. The
runtime's libjxl 0.7 is not used.

**Update (2026-11-07), the snap.** `snap/snapcraft.yaml` builds the same pinned sources as parts
(highway, libjxl, dav1d, libde265, libheif, LibRaw, lensfun, libvips), with the same options.
Their build headers come from Ubuntu 24.04 packages rather than the SDK overlay: core24 and the
`kf6-core24` runtime are built from that release, so the ABI is the one that runs, and the
image libraries the runtime may not carry (libjpeg-turbo, libpng, libtiff, libwebp, LittleCMS,
libexif, brotli) are staged into the snap. The app adds `$SNAP/usr/lib` to its library path
(the runtime's launcher adds only its own folders) and reads the lens database shipped beside
it (`<prefix>/share/lensfun`), since lensfun's own path is where it was built. The KDE Neon 6
extension adds the `network`, `network-bind` and `audio-playback` plugs to every app; PixelFx
uses none of them, and the snap asks for nothing else beyond the desktop set but `home` and
`removable-media`.

## ADR-009: the built-in preset catalog comes from EmulsionPresets (2026-11-05)

**Context.** The owner wants a large preset catalog as a selling point (FEATURES P9), in the
spirit of Lightroom's. Adobe's presets cannot be copied. EmulsionPresets
(github.com/peva3/EmulsionPresets, v3.1.4) is 451 Lightroom presets under the MIT licence, free
for commercial use, with a research note per preset. Its names use film brands, products and
real people's names, and 20 of them are video copies of others.

**Decision.** The catalog is built at development time, not by the build: `scripts/catalog/
build_catalog.py` converts the presets listed in `scripts/catalog/catalog.tsv` with libpixelfx's
own importer (`pfx import`), so they map exactly as an import in the app would, into
`app/src/resources/presets/<category>/*.pfxpreset`, compiled into the app as resources. The table
gives each preset a PixelFx category and a descriptive name without any brand, product or real
person (Portra 400 is "Portrait 400"). Left out: the 20 video copies; 431 remain. The two
lens-profile tools were left out until lens profiles came (ADR-013) and joined Tools on
2026-11-06 (owner). The MIT notice ships with the app (About) and
`REUSE.toml` marks the converted files MIT.

**Consequences.** A new EmulsionPresets release is taken by running the script on it and
reviewing the diff; new presets need a row in the table first. Descriptions are written from
each preset's settings (`core::describeSettings`), so no text of the source is copied. The look of
each preset is the source's intent on PixelFx's engine, not a pixel match with Lightroom
(FEATURES P8); presets that read poorly are dropped from the table after review on real photos.

## ADR-010: compressing files in-process, with our own palette quantiser (2026-11-06)

**Context.** Batch's Compress only (FEATURES M10, PLAN D24) replaces PixelBatch, which ran
jpegoptim, pngquant and gifsicle through a shell. ADR-002 rules out external tools. FEATURES M10
named quantizr (MIT, Rust) through libvips for palettes and cgif for GIF, but quantizr needs a
Rust toolchain in every build (dev, snap, Flatpak, Windows, macOS), and libvips's GIF writer needs
a quantiser as well. The owner chose our own quantiser in libpixelfx (2026-11-06).

**Decision.**
- **Lossless JPEG** through libjpeg-turbo's coefficient API (`jpeg_read_coefficients`,
  `jpeg_write_coefficients`), as jpegtran does: the DCT coefficients are copied, Huffman tables
  optimised, baseline or progressive kept, whichever is smaller. libpixelfx links libjpeg-turbo
  directly (it was already there behind libvips, ADR-003).
- **Our own encoders** for what libvips cannot do without a quantiser: a PNG writer on zlib
  (colour type and bit depth reduction, the five filters, palettes, the original's chunks in their
  required places) and a GIF89a writer with LZW, storing only the changed rectangle of each frame
  when that is exact. libpixelfx links zlib directly.
- **Our own palette quantiser** (`pfx::quantize`): colours binned, split by median cut on the
  largest error, refined by k-means, mapped with damped Floyd-Steinberg diffusion, scored on
  pngquant's 0 to 100 quality scale so a quality floor means what its users expect.
- TIFF and lossless WebP are encoded again by libvips, without loss; lossy WebP, AVIF, HEIC,
  JPEG XL and RAW files are not encoded again (that would lower their quality, or there is no
  encoder); WebP metadata is cleaned in its RIFF chunks without touching the image.
- No zopfli, quantizr, libimagequant or cgif.

**Consequences.** No new third-party code in any build: libjpeg-turbo and zlib are in every
runtime and vcpkg. The quantiser, the PNG writer and the GIF writer are ours to maintain; the
library tests decode every lossless result and compare it pixel for pixel. zopfli can be added
later as an option for slightly smaller PNGs, with a row here and in THIRD_PARTY.

## ADR-011: desktop notifications over Qt D-Bus on Linux (2026-11-06)

**Context.** A batch can take minutes; FEATURES S6 and `batch-running.html` want a notification
when it ends while PixelFx is in the background. On Linux that is a D-Bus call: the notification
portal inside Flatpak, `org.freedesktop.Notifications` elsewhere. The owner approved Qt D-Bus
(2026-11-06).

**Decision.** The app links Qt's D-Bus module (part of Qt, LGPL-3.0, already in every runtime) on
Linux only; `services::Notifier` sends the notification and brings the window forward when it is
clicked (freedesktop's default action; the portal opens the app itself). Windows and macOS get
their own notification APIs with M7; until then they show nothing. The library is untouched.

**Consequences.** The snap needs the `desktop` plug (the KDE extension gives it); the Flatpak
needs no permission for the portal. When no service answers, nothing is shown and nothing fails.

## ADR-012: Camera RAW: LibRaw, and phones' linear DNG read by PixelFx (2026-11-06)

**Context.** RAW is in 2.0 (PLAN D15) through LibRaw (ADR-003). The owner's Galaxy S22 Ultra
writes DNG 1.7: linear (already demosaiced), JPEG XL compressed, with a profile gain table map;
LibRaw reads that only when built with Adobe's DNG SDK. The approved `editor-raw.html` wants a
RAW to open on its camera's look, its embedded JPEG first, and a way out when it cannot be read.

**Decision.**
- Camera RAW files keep going to LibRaw through libvips's `dcrawload`.
- Linear DNG (photometric LinearRaw, uncompressed or JPEG XL) is read by libpixelfx itself
  (`io/dng.cpp`, `io/tiff_reader.cpp`): strips and tiles decoded by libvips (`jxlload`, already in
  the build), then the DNG specification's default rendering steps: black and white levels, the
  colour and forward matrices with camera calibration interpolated for the white, the as-shot
  neutral, the baseline exposure, the active area and default crop, the orientation. The profile
  gain table map is applied for the Camera profile only (`LoadOptions::rawTone`).
- A new profile choice, `camera` (presets version 3, recipes and batch recipes version 4): the
  base curve and colour a RAW needs to look as its camera's JPEG does, on the file's own tone.
- The JPEG inside a TIFF-based RAW is read without developing (`pfx::loadPreview`): thumbnails
  ask for it (`LoadOptions::embeddedPreview`), and a RAW that cannot be developed can still be
  opened as its preview.
- No Adobe DNG SDK, no new dependency.

**Consequences.** Opcode lists, lossless JPEG (compression 7) linear DNGs and floating-point DNGs
go to LibRaw as before. The Camera curve is a global one: a phone's own JPEG may differ where the
phone also tone-mapped locally beyond the gain table map (the owner's test file: a little brighter
and redder flower). Lens profiles: ADR-013.


## ADR-013: lens profiles through Lensfun (2026-11-06)

**Context.** Lens corrections by profile (FEATURES G6) were LATER. The owner asked on 2026-11-06
to write this ADR and build them: Lightroom presets turn them on (`LensProfileEnable`, the two
EmulsionPresets left out of the catalog for it), and photographers expect a lens's distortion
and dark corners to be undone. A profile is a measurement of one lens; nobody can write them
for PixelFx alone. Lensfun is the open database (about 1,000 lenses, used by darktable,
RawTherapee and digiKam) with a library that applies it.

**Decision.**
- libpixelfx links Lensfun (0.3.2 or later; LGPL-3.0, dynamically linked, which CODING_STANDARDS
  section 11 allows), optional at build time: `PFX_WITH_LENSFUN` AUTO (used when found), ON or
  OFF. Without it `pfx::lensProfilesAvailable()` is false and the step does nothing.
- Its database (CC-BY-SA-3.0) ships as data files beside the app, its licence in the notices.
  `pfx::setLensDatabaseFolder` points at a package's own copy; otherwise Lensfun's own search
  (the system's copy, then the user's updates) is used. Updating it is Lensfun's business.
- A new step, `lens_profile` (stage 2, before CA removal): `detail.lensProfile.enable` and two
  amounts, `distortion` and `vignetting` (0 to 200, 100 the profile as measured, as Lightroom's
  scales). Vignetting is undone first on the pixels as the lens drew them, then distortion and
  the profile's lateral chromatic aberration in one bilinear resampling, scaled so no empty
  corner shows. Coordinates are Lensfun's, relative to the image and the sensor, so a preview
  and the full-size export agree.
- The lens is the photo's own: `pfx::RenderOptions::lens` carries its camera and lens from Exif
  (`pfx::lensFacts`), so a preset or a batch only turns the correction on and each photo finds
  its lens, like Lightroom's "LensDefaults". Rendering is a pure function of the image, the
  settings and these facts. A photo with no lens name matches only a fixed-lens camera.
- Presets go to version 4, recipes and batch recipes to version 5; older files read unchanged.
  The Lightroom importer maps `LensProfileEnable`, `LensProfileDistortionScale` and
  `LensProfileVignettingScale`; Lightroom's own profile names are not needed and are ignored.
- The control is Use lens profile in Optics, naming the lens found (`editor-lens-profile.html`,
  approved and built 2026-11-06); the rows are left out when no database could be read.

**Consequences.** Phones are mostly absent from the database (the owner's Galaxy S22 Ultra is);
a phone's DNG describes its own lens in opcodes (WarpRectilinear, FixVignetteRadial) that
ADR-012 leaves to later. Packages carry the library and about 4 MB of database: dev builds
build Lensfun 0.3.4 in `scripts/build-deps.sh`; the snap part, the Flatpak module and the
Windows and macOS builds add it when they are written (M7). The manual lens sliders of
Lightroom (`LensManualDistortionAmount`, vignetting by hand) are not part of this. Lensfun switches the
process's number locale while it searches, so searches run one at a time and each camera and
lens is looked up once. The pixels are upright when corrected: a profile's offset of the lens
centre (rare) is applied along the upright axes, slightly off for a photo shot turned.

## ADR-014: English plural forms through Qt Linguist (2026-11-06)

**Context.** Counts are written `tr("%n photos", nullptr, n)`. Qt chooses the singular or the
plural only through a translation, also for the source language, so with none installed every
count read in the plural: "Process 1 photos", "1 files" (the Batch audit, 2026-11-06). A
hand-made helper ("photo" or "photos") would work for English alone and break the day the app
is translated.

**Decision.** `app/i18n/pixelfx_en.ts` holds only the plural forms (`lupdate -pluralonly`),
filled in by hand and committed. The build compiles it with Qt's `lrelease` into a resource of
the core layer, and `core::installPluralForms` loads it before any text is made. The
`update_translations` target refreshes the file from the sources; new counts get their singular
filled in before the commit. The Linguist tools are build-time only (THIRD_PARTY) and optional:
without them the build warns and counts read in the plural, as before.

**Consequences.** A new count in the code needs `update_translations` and its singular, or it
reads "1 photos" again; the Batch and Compress tests check one count. Real translations, when
they come, load over this file. The Windows CI fetches `qttools`; the other builds already have
it.

