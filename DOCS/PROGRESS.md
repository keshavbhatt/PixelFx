# Progress

## Milestones

| Milestone | Status |
|---|---|
| M0 Analysis and contract | done |
| M1 Library core | done (2026-11-04), performance carried into M3 |
| M2 App skeleton | todo |
| M3 Editor | todo |
| M4 Looks and presets | todo |
| M5 Batch | todo |
| M6 Polish and text | todo |
| M7 Packaging and release | todo |

## Mock approvals

| Mock | Reviewed | Owner |
|---|---|---|
| `editor.html` | 2026-11-04 (`mocks/reviews/editor.md`) | approved 2026-11-04 |
| `heal.html`, `watermark.html`, `export-sizes.html` | 2026-11-04 (`mocks/reviews/`) | approved 2026-11-04 |
| every other mock in `mocks/index.html` | written review still owed | approved 2026-11-04 |
| `batch-compress.html` | 2026-11-04 (`mocks/reviews/batch-compress.md`) | approved 2026-11-04 |

## Sessions (newest first)

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
