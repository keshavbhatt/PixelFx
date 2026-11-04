# Lessons: v1 mistakes and the rule each became

From `reference/analysis-pixelfx-v1.md` section 7. Each rule points at the ADR or document
that enforces it.

| Mistake in v1 | Consequence | Rule |
|---|---|---|
| A `bash` plus Go process per slider release, PNG encoded and decoded on both sides, image on stdout | Seconds of latency, no live preview, a shell command built from strings | Image processing runs in-process through libpixelfx; no child process, no shell (ADR-002) |
| Only the exit code checked, stderr ignored, unsupported formats gave empty output with exit 0 | Silent failures | Every library call returns a typed `Result` with an error code and message; the UI shows the reason (CODING_STANDARDS section 6) |
| Preview on a downscaled copy and export on full resolution with the same pixel-unit parameters | Blur, sharpen, noise and pixelate looked different on export | Every spatial parameter is relative to the image size; a golden test renders at two scales and compares (ADR-004) |
| Processing order fixed in Go, ignoring the UI; gamma always ran | Surprising results, wasted work | The pipeline order is defined once in the library and documented; identity operations are skipped (ADR-004) |
| Integer division in the sigmoid mapping, a jump in colour balance, an inverted PNG slider, a mislabelled Colorize | Sliders that lied | Parameter mappings live in the operation registry with unit tests per mapping; the UI shows the registry's units (ADR-005) |
| No undo, destructive crop | Users reopened the file to recover | Edits are a recipe of settings; undo, history and crop are recipe operations (ADR-004) |
| Full-resolution decode, PNG encode and the histogram on the GUI thread | Freezes on large photos | Decode, render, histogram and encode run on workers; the GUI thread only paints (CODING_STANDARDS section 5) |
| Use-after-free in the async writer; dialogs without parents | Crashes, windows hiding behind the main window | Parent ownership for every QObject; sheets, not top-level dialogs (CODING_STANDARDS section 4, DESIGN section 2) |
| EXIF orientation ignored; EXIF, ICC and text metadata dropped | Rotated photos, colour shifts, lost copyright | Orientation applied on open; ICC honoured; metadata kept by policy on export (FEATURES O4, O5, X4, X5) |
| Vendored code with unclear or non-redistributable licences (Qt Extended EXIF code, ImageCropper, MLV scopes); README said MIT while LICENSE was GPL | Legal risk | No third-party code without an ADR and a `THIRD_PARTY.md` row; `reuse lint` passes (CODING_STANDARDS section 10) |
| `.ui` files, `findChildren` casts of a QSlider subclass without `Q_OBJECT` | Wrong casts, hidden coupling | No `.ui` files; `Q_OBJECT` on every QObject subclass (CODING_STANDARDS section 5) |
| Version 0.0.1 in the app, 1.0 in the snap, "pixelFx" vs "pixelfx" from a qmake ordering bug | Confusing identity | Identity strings and the version live in `app/src/app/version.h.in` and `project(VERSION)` only |
| Snap on core20 (end of life), amd64 only, unpinned source, Qt image format plugins missing | TIFF and WebP broken in the store build | Snap on core24 with the kit's workflow, both arches, pinned sources; a smoke test opens every supported format in the built snap (ROADMAP M7) |
| Temp PNG copies of every photo in `/tmp`, left behind on a crash | Disk use, privacy | No temp copies; the library decodes from the original file |
| PixelBatch (being folded in, PLAN D24) ran jpegoptim, pngquant, gifsicle and SVGO as shell commands and depended on each tool's command line and output text; its interface was weak (owner, 2026-11-04) | Breaks when a tool changes its flags or output, tools missing or mismatched in sandboxes, slow process start per file, errors lost in text | Same rule as row 1: no external tools for image work (ADR-002); PixelBatch contributes a feature list, never code or layout |
