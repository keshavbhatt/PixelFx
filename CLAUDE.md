# PixelFx: project instructions

PixelFx 2 is a from-scratch rewrite of a quick, cross-platform photo retouching app: looks
(filters), presets, custom looks and presets, and batch editing of many photos at once. It is a
monorepo (ADR-001):

- `libs/pixelfx-core/`: libpixelfx, the reusable image library. Standalone CMake project,
  C++20, no Qt, Apache-2.0, namespace `pfx`. Replaces the old Go `pixel` tool.
- `app/`: the Qt 6.11 / C++20 / CMake Widgets client, GPL-3.0-or-later, with the Ktechpit
  licensing module under its own licence (Model B, `LICENSING.md`).

Read `DOCS/` first: `PLAN.md` (architecture, OPEN questions in section 9), `FEATURES.md` (scope
contract), `DECISIONS.md` (ADRs), `DESIGN.md` (tokens, layout, review method), `presets.md`
(file formats), `CODING_STANDARDS.md` (binding), `PROGRESS.md` (log). Conventions come from
the owner's reusable rewrite kit, kept outside this repository (read-only). PixelFx 1 is on the
`legacy-v1` branch (audited in `DOCS/reference/analysis-pixelfx-v1.md`). `DOCS/private/` holds
business notes and is never committed.

## Build and test

```sh
# The app and the library against the KDE snap SDK, run against the kf6-core24 runtime
# (needs the kde-qt6-core24-sdk, kf6-core24, core24 and cmake snaps, and uv for meson; ADR-008)
PIXELFX_CMAKE_ARGS="-DPIXELFX_WERROR=ON" scripts/dev-build.sh --tests
scripts/dev-run.sh -- photo.jpg
QT_QPA_PLATFORM=offscreen PIXELFX_DEBUG_OPEN=photo.jpg PIXELFX_DEBUG_GRAB=shot.png scripts/dev-run.sh

# The library alone, on the host's libraries
make test      # builds libpixelfx with -Werror and runs its tests (build/lib)
make lint      # clang-format check and clang-tidy
make golden    # regenerate golden images on purpose, then review them
```

Debug hooks are listed at the top of `app/src/main.cpp`. For manual runs, point
`XDG_CONFIG_HOME`, `XDG_DATA_HOME` and `XDG_CACHE_HOME` at a scratch folder or use `--profile`.
Neither moves the Pictures folder, where Export and Batch save by default: also write
`XDG_PICTURES_DIR="<scratch>/pics"` to `$XDG_CONFIG_HOME/user-dirs.dirs`, or the output lands
in the owner's `~/Pictures/PixelFx`.

## Standing rules

- No em dashes or en dashes anywhere. No commit or PR attribution lines. Commit style:
  `area: plain sentence` or `area, area: plain sentence` (areas: `lib`, `app`, `ui`, `docs`,
  `mocks`, `snap`, ...).
- Never mention accounts or licensing in user-facing release text (`CHANGELOG.md`, metainfo
  release notes, the What's new sheet, README store copy); that work is described only in
  commit messages and `DOCS/`. `tst_changelog` enforces it.
- Mocks before screens: every screen or visible UI change first gets an HTML mock in
  `DOCS/mocks/` and a written review in `DOCS/mocks/reviews/` (method in `DOCS/DESIGN.md`
  section 5, measured numbers, not adjectives); then the owner approves it. Approval is
  recorded in `DOCS/mocks/index.html` and `DOCS/PROGRESS.md`. No screen is coded from a
  description alone. The mocks are plain HTML; keep them so.
- libpixelfx never includes Qt, never depends on `app/`, and takes no GPL dependency
  (CODING_STANDARDS section 11). New dependencies anywhere need an ADR and a `THIRD_PARTY.md`
  row.
- Images are processed in-process by libpixelfx; never spawn a process or a shell to process
  an image (LESSONS row 1).
- Every spatial parameter is relative to the image size; preview and export must match.
- Any change to a file format in `DOCS/presets.md` bumps its `version` and adds a migration and
  a test in the same commit.
- Open questions in `DOCS/PLAN.md` section 9 are the owner's: ask, do not assume.
- The snap is built by GitHub Actions only, the Flatpak by Flathub's CI only; never push to a
  Flathub fork or open a Flathub PR without the owner's explicit consent.
- Verify headlessly (offscreen grabs, `PIXELFX_DEBUG_*` hooks) before asking the owner to look;
  use a scratch `--profile`, never the owner's settings or photos.
- Test images in the repository must be owned or public domain, with provenance in
  `fixtures/README.md`.
- Run the library tests, `ctest` and the linters before declaring a task done.
- Release checklist: date the changelog heading, add the metainfo release, bump
  `project(VERSION)`, update the snap description.
