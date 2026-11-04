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

## Mocks (`DOCS/mocks`, not shipped)

| Component | Version | Licence | Use |
|---|---|---|---|
| Lucide icons | snapshot ba6751ac (2026-09-18), from the rewrite kit | ISC | Glyphs in the HTML mocks |
