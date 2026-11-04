# Roadmap

Milestones with exit criteria. The library leads; packaging is deliberately last.

| Milestone | Exit criteria |
|---|---|
| M0 Analysis and contract | `reference/analysis-pixelfx-v1.md`, `research/image-libraries.md`, PLAN section 9 answered (done 2026-11-04 except Q7, Q10b, Q4b), FEATURES with every row decided, LESSONS, ADR-000 to ADR-006, DESIGN tokens, every M3 to M5 mock approved |
| M1 Library core | `libs/pixelfx-core` builds standalone with `-Werror` on Linux; image buffer, colour conversions, recipe and operation registry with JSON round trips and migrations; the Light, Color, Curves and Detail operations with golden tests at two scales; JPEG, PNG, TIFF, WebP in and out; `pfx` CLI applies a recipe to a file |
| M2 App skeleton | Layered app libraries build with `-Werror` against the KDE snap SDK; settings facade with tests; single instance and CLI; the main window opens a photo and renders it through the library; `dev-run.sh` runs against the snap runtime; smoke test offscreen; Windows (MSVC, vcpkg) and macOS builds and tests in CI (PLAN D19) |
| M3 Editor | libpixelfx renders in tiles inside libvips (ADR-003) with SIMD: a 24 MP export in under 1.5 s and memory bounded (M1 measured 2.5 s and 1.1 GB for 20 MP rendering the whole image at once); dehaze; fuzzed parsers and decoders. Every C and F row KEEP `done`: live preview, histogram, undo and history, before and after, crop and geometry, sidecars, Export sheet; HEIC, AVIF, JPEG XL, RAW in; colour management end to end |
| M4 Looks and presets | Every D and E row KEEP `done`: built-in looks, look editor, LUT import and bake, preset library, `.xmp` import, filmstrip selection and paste |
| M5 Batch | Every G row KEEP `done`: Batch page, background runner with memory cap, recipes, command line batch; a 500-photo batch completes within the memory budget |
| M6 Polish and text | Sheets everywhere, shortcuts sheet, About with diagnostics, Report a bug, What's new, Online guide, settings pages; CHANGELOG, README, GUIDE, metainfo for search, screenshots (dark and light) |
| M7 Packaging and release | Snap built in CI (amd64, arm64) on edge with a format smoke test; Flathub manifest lint-clean and submitted by the owner; release checklist done. Windows and macOS store releases follow the Linux 2.0 (PLAN D19) |
