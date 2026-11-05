# Third-party components

Every dependency, why it is here, and its licence. A new dependency needs an ADR in
`DOCS/DECISIONS.md` and a row here (CODING_STANDARDS section 10).

## libpixelfx (`libs/pixelfx-core`, Apache-2.0)

| Component | Version | Licence | Use | How it is obtained | ADR |
|---|---|---|---|---|---|
| libvips (with its C++ API) | 8.15 or later (8.18 in the snap) | LGPL-2.1-or-later, linked dynamically | Decoding, encoding, orientation, resizing | System package; snap part and Flatpak module at release | ADR-003 |
| LittleCMS | 2.14 or later | MIT | Colour conversion through ICC profiles | System package | ADR-003 |
| libjpeg-turbo | 2.1 or later | IJG, BSD-3-Clause and zlib, linked dynamically | Lossless JPEG optimisation through the coefficient API (Compress only) | System package; the runtimes and vcpkg have it | ADR-010 |
| zlib | 1.2.11 or later | Zlib, linked dynamically | Deflate for the PNG writer, inflate for compressed PNG text | System package; the runtimes and vcpkg have it | ADR-010 |
| Lensfun | 0.3.2 or later (0.3.4 in dev builds) | LGPL-3.0, linked dynamically; its lens database CC-BY-SA-3.0, shipped as data | Lens profiles: a lens's distortion, lateral chromatic aberration and vignetting, found from the photo's camera and lens (optional at build time) | System package; dev builds: `scripts/build-deps.sh` (pinned, SHA-256); a snap part and a Flatpak module at release | ADR-013 |
| nlohmann/json | 3.12.0 | MIT | Reading and writing PixelFx documents (private, header-only) | System package, or fetched at a pinned SHA-256 | ADR-007 |
| doctest | 2.4.12 | MIT | Library tests only, not shipped | System package, or fetched at a pinned SHA-256 | ADR-007 |

## The app (`app/`, GPL-3.0-or-later)

| Component | Version | Licence | Use | How it is obtained | ADR |
|---|---|---|---|---|---|
| Qt | 6.11 | LGPL-3.0 (and GPL-3.0 for some tools), linked dynamically | The UI; Qt D-Bus on Linux for notifications (ADR-011) | `kde-qt6-core24-sdk` snap (dev), the KDE runtimes (packages), install-qt-action (CI) | ADR-000 |
| Lucide icons | snapshot ba6751ac (2026-09-18) | ISC | UI glyphs in `app/src/resources/icons/ui` (notice alongside) | Pulled from the rewrite kit's vendored set | ADR-000 |
| EmulsionPresets | v3.1.4 (2026-11-04) | MIT | The built-in preset catalog in `app/src/resources/presets`: 429 of its Lightroom presets converted by the library's importer and renamed (`scripts/catalog`) | The release ZIP of github.com/peva3/EmulsionPresets, converted at development time; not fetched by the build | ADR-009 |
| libvips | 8.18.7 for dev builds | LGPL-2.1-or-later | As for the library; dev builds compile it against the snap SDK | `scripts/build-deps.sh` (pinned, SHA-256) | ADR-008 |
| libheif | 1.23.7 for dev builds | LGPL-3.0-or-later, linked dynamically | HEIC and AVIF reading in dev libvips (no encoders, no plugin loading) | `scripts/build-deps.sh` (pinned, SHA-256) | ADR-003, ADR-008 |
| libde265 | 1.1.3 for dev builds | LGPL-3.0-or-later, static inside libheif | HEVC decoding for HEIC | `scripts/build-deps.sh` (pinned, SHA-256) | ADR-003, ADR-008 |
| dav1d | 1.5.4 for dev builds | BSD-2-Clause, static inside libheif | AV1 decoding for AVIF | `scripts/build-deps.sh` (pinned, SHA-256) | ADR-008 |
| libjxl | 0.12.0 for dev builds | BSD-3-Clause, linked dynamically | JPEG XL reading and writing in dev libvips | `scripts/build-deps.sh` (pinned, SHA-256) | ADR-003, ADR-008 |
| highway | 1.4.0 for dev builds | Apache-2.0 or BSD-3-Clause, static inside libjxl | SIMD for libjxl | `scripts/build-deps.sh` (pinned, SHA-256) | ADR-008 |
| brotli | 1.1.0 (the SDK's) | MIT, linked dynamically | Compressed boxes in libjxl | `kde-qt6-core24-sdk` and `kf6-core24` snaps | ADR-008 |
| LibRaw | 0.22.3 for dev builds | LGPL-2.1-only or CDDL-1.0, linked dynamically | Camera RAW reading in dev libvips | `scripts/build-deps.sh` (pinned, SHA-256) | ADR-003, ADR-008 |

## Mocks (`DOCS/mocks`, not shipped)

| Component | Version | Licence | Use |
|---|---|---|---|
| Lucide icons | snapshot ba6751ac (2026-09-18), from the rewrite kit | ISC | Glyphs in the HTML mocks |
