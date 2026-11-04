# Third-party components

Every dependency, why it is here, and its licence. A new dependency needs an ADR in
`DOCS/DECISIONS.md` and a row here (CODING_STANDARDS section 10).

## libpixelfx (`libs/pixelfx-core`, Apache-2.0)

| Component | Version | Licence | Use | How it is obtained | ADR |
|---|---|---|---|---|---|
| libvips (with its C++ API) | 8.15 or later (8.18 in the snap) | LGPL-2.1-or-later, linked dynamically | Decoding, encoding, orientation, resizing | System package; snap part and Flatpak module at release | ADR-003 |
| LittleCMS | 2.14 or later | MIT | Colour conversion through ICC profiles | System package | ADR-003 |
| nlohmann/json | 3.12.0 | MIT | Reading and writing PixelFx documents (private, header-only) | System package, or fetched at a pinned SHA-256 | ADR-007 |
| doctest | 2.4.12 | MIT | Library tests only, not shipped | System package, or fetched at a pinned SHA-256 | ADR-007 |

## The app (`app/`, GPL-3.0-or-later)

| Component | Version | Licence | Use | How it is obtained | ADR |
|---|---|---|---|---|---|
| Qt | 6.11 | LGPL-3.0 (and GPL-3.0 for some tools), linked dynamically | The UI | `kde-qt6-core24-sdk` snap (dev), the KDE runtimes (packages), install-qt-action (CI) | ADR-000 |
| Lucide icons | snapshot ba6751ac (2026-09-18) | ISC | UI glyphs in `app/src/resources/icons/ui` (notice alongside) | Pulled from the rewrite kit's vendored set | ADR-000 |
| libvips | 8.18.7 for dev builds | LGPL-2.1-or-later | As for the library; dev builds compile it against the snap SDK | `scripts/build-deps.sh` (pinned, SHA-256) | ADR-008 |

## Mocks (`DOCS/mocks`, not shipped)

| Component | Version | Licence | Use |
|---|---|---|---|
| Lucide icons | snapshot ba6751ac (2026-09-18), from the rewrite kit | ISC | Glyphs in the HTML mocks |
