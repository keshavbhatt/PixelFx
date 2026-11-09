# Progress

## Milestones

| Milestone | Status |
|---|---|
| M0 Analysis and contract | done |
| M1 Library core | done (2026-11-04), performance carried into M3 |
| M2 App skeleton | done locally (2026-11-04); Windows and macOS CI written, first run after the next push |
| M3 Editor | wip (live editing, undo, before and after started 2026-11-05) |
| M4 Looks and presets | wip (presets, Lightroom import, looks, the look editor, LUTs: 2026-11-05) |
| M5 Batch | wip (Batch page, runner, recipes, watermark: 2026-11-05) |
| M6 Polish and text | todo |
| M7 Packaging and release | todo |

## Mock approvals

| Mock | Reviewed | Owner |
|---|---|---|
| `editor.html` | 2026-11-04 (`mocks/reviews/editor.md`) | approved 2026-11-04 |
| `heal.html`, `watermark.html`, `export-sizes.html` | 2026-11-04 (`mocks/reviews/`) | approved 2026-11-04 |
| every other mock in `mocks/index.html` | written review still owed | approved 2026-11-04 |
| `batch-compress.html` | 2026-11-04 (`mocks/reviews/batch-compress.md`) | approved 2026-11-04 |
| `editor-grading.html` | 2026-11-05 (`mocks/reviews/editor-grading.md`) | approved 2026-11-05 (as drawn: two wheels, a group of its own) |
| `about.html`, `whats-new.html` | 2026-11-05 (`mocks/reviews/about-whats-new.md`) | approved 2026-11-05 |
| `editor-value-entry.html` | 2026-11-05 (`mocks/reviews/editor-value-entry.md`) | approved 2026-11-05 |
| `preview-viewer.html` | 2026-11-05 (`mocks/reviews/preview-viewer.md`) | approved 2026-11-05 (built with the editor's zoom bar; the counter in the title) |
| `editor-develop.html` | 2026-11-05 (`mocks/reviews/editor-develop.md`) | approved 2026-11-05 (supersedes the two-wheel grading of `editor-grading.html`) |
| `editor-presets-catalog.html` | 2026-11-05 (`mocks/reviews/editor-presets-catalog.md`) | approved 2026-11-05 |
| `editor-mixer-target.html` | 2026-11-05 (`mocks/reviews/editor-mixer-target.md`) | approved 2026-11-05 |
| `editor-collapse-all.html` | 2026-11-05 (`mocks/reviews/editor-collapse-all.md`) | approved 2026-11-05 |
| `editor-zoom-loading.html` | 2026-11-05 (`mocks/reviews/editor-zoom-loading.md`) | approved 2026-11-05 |
| `editor-looks-preview.html` | 2026-11-05 (`mocks/reviews/editor-looks-preview.md`) | approved 2026-11-05 |
| `editor-zoom-loading.html`, slow preview | 2026-11-05 (same review, "Slow preview") | approved 2026-11-05 (the ring always turns) |
| `batch-convert-rename.html` | 2026-11-06 (`mocks/reviews/batch-convert-rename.md`) | approved 2026-11-06 ("Batch threads" of `settings.html` became "Photos at a time") |
| `editor-raw.html` | 2026-11-06 (`mocks/reviews/editor-raw.md`) | approved 2026-11-06 (Kelvin for RAW only) |
| `editor-presets-catalog.html`, hover preview | 2026-11-05 (same review, "Hover preview, reworked") | approved 2026-11-05 |
| `editor-lens-profile.html` | 2026-11-06 (`mocks/reviews/editor-lens-profile.md`) | approved 2026-11-06 (the two EmulsionPresets lens tools join the catalog) |
| `close-photo.html` | 2026-11-06 (`mocks/reviews/close-photo.md`) | approved 2026-11-06 (with Remember edits off, closing an edited photo asks first) |
| `batch-empty.html`, `batch-recipe-picker.html` | 2026-11-06 (`mocks/reviews/batch-empty.md`, `batch-recipe-picker.md`) | approved 2026-11-06 |
| `batch.html` and its states, without the list or grid switch | 2026-11-06 (the owner's call) | approved 2026-11-06 (the grid showed less than the list: no size, no status, no reason a photo was not saved) |
| `shortcuts.html`, the rows of bound keys it lacked; `export.html`, the overwrite warning line | 2026-11-06 (PROGRESS, testing every feature) | approved 2026-11-06 |
| `photo-details.html`, `-plain`, `-location` | 2026-11-06 (`mocks/reviews/photo-details.md`) | approved 2026-11-06 (the shortcut I; a long folder path drops whole folders from its middle) |
| `crop-corners.html`, `-done` | 2026-11-06 (`mocks/reviews/crop-corners.md`) | approved 2026-11-07 (both results, Whole photo first) |
| `report-bug.html`, `crash-notice.html` | 2026-11-07 (`mocks/reviews/report-bug.md`) | approved 2026-11-07 (reports go to GitHub issues only) |
| `pro-sheet.html`, `pro-gate.html`, `pro-export.html`, `pro-trial-end.html` | 2026-11-08 (`mocks/reviews/pro.md`) | approved 2026-11-08 (the free limit of 10 photos per run; no trial recap) |

## Waiting for the owner (2026-11-05)

Decisions the work stopped at; everything else went ahead within the approved mocks.

1. Built-in looks (13) and presets (4): the values are drafts by eye. Decided 2026-11-06
   (owner): keep them, the owner tunes them before the release.
2. Kelvin white balance presets: decided 2026-11-06 (owner), RAW only, as now and as
   Lightroom. A JPEG or PNG has its white balance baked in, so a named light source cannot be
   right for it; those photos keep As shot, Auto, the picker, Temp and Tint.
3. Theme: switches live, done 2026-11-06 (owner).
4. macOS CI skips one curve-editor test (a crash only on the runner, clean under ASan locally).
5. Real Lightroom `.xmp` exports should be tried on the importer (EmulsionPresets' 451 so far,
   2026-11-05; a second, real-world set is still wanted for the 95% target of FEATURES P8).
6. Batch writes beside each original into a folder named "PixelFx": decided 2026-11-06
   (owner), keep the name.
   (The catalog's names and Tools: decided 2026-11-05, see below.)
7. Zoom has a FEATURES row now (E17b, 2026-11-06).
8. Small departures from approved mocks, until their parts exist: the look
    editor previews on This photo and a colour ramp (no Portrait or Landscape test photos are
    shipped); the main menu leaves out Save edit with the file;
    Copy to clipboard has no shortcut (the mock's
    Ctrl+Shift+C is Copy settings).
9. Still to plan: monitor profile, GPU preview; sidecars next to the photo later (owner,
    2026-11-06). The batch-end notification is done on Linux (ADR-011).
10. Online guide: done 2026-11-06, the GitHub wiki (main menu and About).
11. Small choices made with the approved About, What's new and grading mocks, easy to change:
    What's new does not open on a first install (nothing is new yet), only after an update from
    an earlier version, PixelFx 1 included; Diagnostics leaves out "memory for previews" (there
    is no such limit yet); the Color grading group starts closed, like the Color mixer; the
    open-source notices list what PixelFx ships (THIRD_PARTY.md), not the licensing module.
12. Small choices made in Heal, easy to change: while Heal is on, the photo shows upright and
    uncropped (spots live in that frame, so a later crop never moves them; the mock shows no
    crop either way); spots are named "Spot 1", "Brush stroke 2" rather than by place ("Spot in
    the sky" in the mock needs scene recognition); Reset takes off the spots made since Heal
    opened and Done keeps them (each spot is also its own history step, and undo works in
    Heal); Reveal dust explains itself in its tooltip rather than a first-time line.
13. Approved by the owner 2026-11-06: the Keyboard shortcuts sheet and `shortcuts.html` gained
    the rows of keys that were bound but not listed (New look, Quit, Crop's Enter, Escape and
    X, Heal's Delete); Export shows Batch's overwrite warning line when Overwrite, the photo's
    own folder and {name} would write over the original.
14. Kept apart from the mocks after the pixel pass (reasons in
    `mocks/reviews/drift-2026-11-06.md`): AVIF only where the build writes it, auto straighten
    and the four-corner keystone not built yet, the grading wheels in true hues, the curve line
    1.6 px, the Effects Style list 3 px and Heal's switches 2 px taller than drawn.
15. Pushing: not yet (owner, 2026-11-06); commits stay local until the owner pushes.
16. Next: the photo information panel (FEATURES O7); its mock approved 2026-11-06, to be built.

## Sessions (newest first)

### 2026-11-10 (the Ktechpit Pass line in PixelFx Pro)

- The owner asked for the Pass line in the PixelFx Pro sheet to offer the pass rather than ask
  people to add PixelFx to one. Mock `pro-sheet.html` (pin 4) and its review
  (`mocks/reviews/pro.md`, revision 2026-11-10); the owner approved it as is.
- Built: "PixelFx is included in Ktechpit Pass" with Get the Pass, which opens the Pass checkout
  with this Account ID and `source=in_app_pixelfx` (`LicenseService::passCheckoutUrl`), so a pass
  bought from it unlocks PixelFx at once; "Have one already? Add PixelFx to it" still opens the
  portal for a pass bought with another app's Account ID. `tst_pro` checks the link.
- Verified: dev build with -Werror, 45 of 45 tests, `make lint`; the sheet grabbed offscreen in
  both themes on a large virtual screen. On screens under about 830 px the sheet scrolls by a
  line, as sheets do.
- PixelFx 2.0.1 prepared: the changelog section dated 2026-11-10 (links in windows work again),
  the metainfo release, `project(VERSION 2.0.1)`; the snap description is unchanged, as nothing
  new is offered. The release text names only the link fix (`tst_changelog` passes); the Pass
  line is in this log and the commit messages only.

### 2026-11-08 (PixelFx Pro, the mocks)

- Four mocks for Pro, reviewed in `mocks/reviews/pro.md`: the PixelFx Pro sheet from the main
  menu (trial, Free, Pro), the prompt when a paid feature is used, try freely and pay to save in
  Export, and the trial's end. DESIGN section 4: release text never mentions Pro or licensing;
  the app keeps Pro quiet (no prompt at start, no banner, nothing on a free export).
- The owner approved them: Batch and paste stay free for 10 photos per run, and the trial
  recap is dropped, so PixelFx counts nothing about how the trial was used.
- Built (ADR-016): the shared AccountAndLicense module, `services::LicenseService` (Account ID,
  14-day trial, the server's last answer, checkout links with `source`), `core::licensing`
  (what a photo or a batch uses of Pro), `ui::ProGate` and `ProPrompt`, the PixelFx Pro sheet,
  the main menu entry, Export's "This photo uses Pro" box with Remove, the trial's end note, the
  Pro mark (Export and Batch Watermark, Bake to .cube, the Export items of the preset and look
  menus). Gated: Batch, Process the rest, Export of several photos and paste or apply to the
  selection above 10 photos (the first 10 free), saving a RAW, an imported Lightroom preset, a
  LUT look or a watermark (Export, Copy to clipboard, Batch), sharing looks and presets as
  files, Bake to .cube, and `pixelfx --batch` (one line, exit code 3).
- An imported Lightroom preset now names itself in the settings (`source.lightroomPreset`):
  presets version 6, recipes 7, batch recipes 8 (presets.md); older files read unchanged.
- The snap has the `network` plug; the Flathub draft (not pushed) has `--share=network`.
- Verified headlessly in a scratch profile: the Pro sheet in its three states, the prompt, the
  menu, Export on Free, the trial's end and the Batch page (Debug build for Free and Pro);
  `tst_pro` and the runner's limit in `tst_batch`; library 174 and app 45 tests pass.
- Left for the owner: the Pro mark on the preset and look tiles themselves is not drawn (the
  approved note names "the Presets panel", but the panel is tiles now, not the list in the mock);
  a small mock first if it is wanted.

### 2026-11-07 (the same libraries in every build)

- First pushes: CI's Linux job failed on a libvips 8.15 API difference (a test fixed); the snap
  failed on arm64 (KDE publishes the SDK snap for amd64 only) and on amd64 in LibRaw's configure
  (the extension's library path pointed into the SDK). The owner asked for the same libraries in
  every build, built from source where needed: ADR-015.
- `scripts/deps-pins.sh` holds every pin (Qt 6.11.2, the source-built libraries with their
  checksums, the image libraries at Ubuntu 24.04's versions). Linux CI and the snap now run
  `build-deps.sh` and `dev-build.sh` as a dev build does, on the KDE SDK and runtime snaps; the
  snap no longer builds its own parts or stages Ubuntu packages, and is amd64 only for now.
  `build-deps.sh` stops when the SDK's Qt or image libraries leave the pins (checked by changing
  the Qt pin). Windows and macOS build every library through vcpkg at the pinned versions, with
  `packaging/vcpkg/ports` for libvips (HEIF, JPEG XL, RAW, GIF, PPM, zlib) and LibRaw (no
  JasPer); macOS moves from Homebrew to vcpkg, dynamically linked. `scripts/check-pins.py` runs
  first in Linux CI. Qt 6.11.2 everywhere.
- Proven on GitHub: CI passes on Linux, Windows and macOS (run 42362119916, commit a1d1bdc) and
  the snap built through the scripts is on the Snap Store's edge channel (revision 19,
  `2.0.0+gita1d1bdc`, 10.3 MB), published by the workflow after CI. Fixed on the way:
  - vcpkg: zlib 1.3 with Xcode 16 (a port with zlib 1.3.1's one-line fix); pcre2's port names
    the sljit commit instead of asking GitHub's API, which refuses CI runners without a login;
    lensfun installed on ARM macOS with `--allow-unsupported`.
  - MSVC: the style sheet in literals under 16 KB, `NOMINMAX` before `windows.h`.
  - Windows bugs found by the tests: Compress only kept the original open while replacing it,
    and a file libvips had read could not be written over; `pfx::closeFiles()` empties libvips'
    cache before a save over an existing file (it first used `vips_cache_drop_all`, which frees
    the cache for good; a test now catches that).
  - Tests made portable: read-only folders (Windows ignores them), lensfun's SSE and plain C
    paths compared by mean, Qt's offscreen platform pointed at the system fonts on Windows, Qt's
    own file dialog left out of the Windows monkey run (it crashes in its file thread offscreen).
  - The snap: PixelFx built in `build/app` (in `app/`, its sources, Qt's CMake helpers recursed
    for ever); the workflow builds amd64 only; the unused plain libraw left out.
- The snap's `contact` is connect@ktechpit.com and its `donation` the PayPal link with 11 filled
  in (the owner's choice, 2026-11-08). The stable channel still holds PixelFx 1.0 until a
  `v2.0.0` tag.

### 2026-11-07 (the snap, and the mocks index)

- The mocks index said "Approved" on screens built since; every card now says whether its screen
  is built and checked against the mock by pixel (with the kept differences named), the
  superseded grading mock is greyed, the three state mocks it did not link are linked, and the
  stale "Later" section (About, What's new, Report a bug, all built) is gone.
- Packaging for Linux, from the install tree: `cmake --install` puts `bin/pixelfx`, the desktop
  file (`Exec=pixelfx %F`, every format PixelFx reads as a MIME type, RAW makes included), the
  AppStream metainfo (summary and keywords of PLAN Q11, branding colours of the icon, no word
  on accounts or licensing) and the icons (PNG 16 to 512 rendered from the SVG, the scalable
  and symbolic SVGs) into `share/`. `desktop-file-validate` and `appstreamcli validate` pass.
- `snap/snapcraft.yaml`: core24, the KDE Neon 6 extension, amd64 and arm64; the codecs as parts
  at the pins of `build-deps.sh` (ADR-008 update); `home` and `removable-media`; the version from
  `project(VERSION)`, with `+git<hash>` off a tag. `.github/workflows/snap.yml` builds it on
  GitHub for both architectures and publishes straight to the Snap Store, edge (main) or stable
  (a `v` tag matching the version), as Omnidict does, once `SNAPCRAFT_STORE_CREDENTIALS` and
  `PUBLISH_TO_STORE` are set; no build is kept as a GitHub artifact (owner, 2026-11-07). Not built here (the snap is built by GitHub Actions only):
  `snapcraft expand-extensions` checks the file; the first real build is the first push.
- CI made cheaper (owner, 2026-11-07): a push or pull request that changes only `DOCS/`,
  `README.md`, `CLAUDE.md` or `LICENSING.md` starts no CI; the snap workflow no longer starts on
  its own for main but after CI passes there (`workflow_run`, on the commit CI tested), so a
  failing commit never reaches edge; version tags still start it directly. Every CI job keeps
  its compiled objects in ccache (Windows with the debug information inside the objects,
  `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded`, which ccache can cache).
- The app reads a lens database shipped beside it (`<prefix>/share/lensfun/version_1`).
- `tst_changelog` now also keeps accounts and licensing out of the metainfo and the snap's
  store text (checked to fail with a forbidden word in the description).

### 2026-11-07 (Report a bug and the crash handler, built)

- Approved by the owner, reports to GitHub issues only. Built:
  - `core::crash`: on Linux and macOS, handlers for SIGSEGV, SIGBUS, SIGFPE, SIGILL and SIGABRT
    on a stack of their own (a stack overflow still writes), writing with async-signal-safe calls
    only into a file opened by path at start: the kind, the time, the photo's name (kept in a
    fixed buffer when a photo opens) and `backtrace_symbols_fd`; then the default action (core
    dump, exit code). An uncaught exception ends as an abort. Windows: an unhandled exception
    filter writes the same without the stack (built by CI; not run here). The executable
    exports its symbols on Linux (`ENABLE_EXPORTS`) so the stack names PixelFx's functions.
  - The next start reads `crash.txt`, moves it to `crash-previous.txt` (shown once), and shows
    the notice before What's new. The report's crash text has the kind, the time, the version,
    the stack (library folders left out, names demangled) and the log lines of the run that
    crashed, never the photo's name.
  - `core::bug_report` (pure): file and folder names in log lines replaced by their kind, the
    run before this one in the log, the issue's title and text, its address (encoded once: a
    test caught the text percent-encoded twice) and the clipboard fallback past 6000
    characters.
  - `ReportBugSheet` and `CrashNotice` from the mocks; About's Report a bug opens the sheet.
  - Checks: `tst_bug_report` (names left out, the run before, the issue, a crash file read,
    a real crash: a child process installs the handlers and crashes, and its file is read and
    moved aside, the sheet's attachments, the notice's wording); 44 app suites; the app crashed
    on purpose with `PIXELFX_DEBUG_CRASH` wrote its file and the next start read it; grabs of
    both sheets against the mocks. Debug: `PIXELFX_DEBUG_SHEET=report|report-crash|crash`.

### 2026-11-07 (Report a bug and the crash notice, the mocks)

- The owner's next choice: FEATURES S7, the crash handler and a Report a bug sheet.
  `report-bug.html` (what happened, the attachments each with Show, a filled-in GitHub issue
  opened in the browser, Copy report) and `crash-notice.html` (the next start after a crash:
  what happened, what was kept, Report the problem), with the review. Waiting for the owner.

### 2026-11-07 (Fix by four corners, built)

- The owner approved `crop-corners.html` with both results, Whole photo first. Built:
  - libpixelfx: a projective warp (Heckbert's square to quadrilateral) after straightening and
    before the crop, in the one geometry pass; Inside the corners keeps the shape at its average
    sides, Whole photo the largest rectangle of the photo's shape around the shape's middle
    (the page stays whole; first tried around the warped photo's middle, which cut the page),
    falling back to Inside when part of the photo would go past infinity.
    `pfx::cornersUsable` (convex, clockwise, sides of 1%) is shared by the library and the drag.
    While corners are set the keystone sliders are ignored. Presets 5, recipes 6, batch recipes
    7, documented in `DOCS/presets.md`, with tests (a shape seen at an angle comes back square
    in both modes, folded or mirrored corners refused, the versions read and written).
  - The app: `CornerTool` on the canvas (handles, the shape's outline and thirds, the outside
    dimmed, a magnifier three times the photo beside the dragged corner; Tab and the arrow keys,
    taken before the window's shortcuts), the panel's Fix by four corners / Edit the four
    corners, Reset corners / Remove and Keep, the sliders off while corners are set, Enter and
    Escape applying or leaving the corners while placing. Turns and flips carry the corners
    (re-labelled so the first is still the top left). New corners start the crop again on the
    new picture. `Document::setCornerEditing` shows the straightened photo without corners or
    crop while they are placed.
  - Debug: `PIXELFX_DEBUG_CORNERS`. Checks: 173 library tests, 43 app suites (the panel, the
    canvas drag and keys, the window's Escape, Enter and Done, turns and flips); grabs of the
    three states against the mocks. `tst_looks_panel` failed once under a parallel run and
    passed alone and in two more full runs.

### 2026-11-06 (Fix by four corners, the mock)

- Next after Photo details: FEATURES G5's four-corner fix, whose button `crop.html` draws but
  whose use it never did. `crop-corners.html` (placing the corners: handles, the shape's
  outline and thirds, the outside dimmed, a magnifier at the dragged corner, Keep: Whole photo
  or Inside the corners, the sliders off) and `crop-corners-done.html` (after applying), with
  the review. Waiting for the owner; nothing is built. Building it changes the recipe format
  (`geometry.perspective.corners` and `keep`): a version, a migration and a test.

### 2026-11-06 (Photo details, built)

- The approved `photo-details.html` built: `PhotoDetailsPanel`, opened by the header's file name
  (now a button with a chevron, the "open" shade while the panel shows), I, and View > Photo
  details; it stays open while stepping through photos and follows the photo, Esc, I or its ×
  close it and give the focus back; it scrolls when the window is shorter than it.
- libpixelfx: `ExifSummary` reads where the photo was taken from the GPS block (a block without
  a fix, 0 and 0, or out of range is no location), and `pfx::profileName` reads a colour
  profile's own name through Little CMS; both tested (169 library tests).
- Camera makes are said as people say them, everywhere the camera is named (the panel, the
  histogram's tooltip, watermark and Batch {camera}): "Fujifilm X-T4", "Nikon Z 6_2", "Olympus
  E-M5" for FUJIFILM, NIKON CORPORATION and OLYMPUS IMAGING CORP.
- Paths in a label (Export's, Batch's and Compress only's folders, the panel's) keep their start
  and their last folder and drop whole folders from the middle (`PathLabel::fitted`), as approved
  for the panel; before, they were cut mid-word.
- Copy details leaves the coordinates out until Show, so a pasted bug report does not carry
  them by accident.
- Checked against the mock by pixel: every line within 1 to 3 px; light and dark; a 1024 by 640
  window. `tst_photo_details` (the words, the path rule, the panel, I and Esc, the header's name);
  43 app suites pass. Debug: `PIXELFX_DEBUG_DETAILS=1|location`.

### 2026-11-06 (Photo details, the mock)

- The mock of FEATURES O7, the owner's choice for the next work: `photo-details.html` with two
  more states (`-plain`, a file without Exif; `-location`, the location shown) and its review.
  The file name in the header opens a panel over the photo (also I and View > Photo details):
  File (size, format and bit depth, file size, colour profile), Camera (camera, lens, exposure,
  focal length, date taken), Location behind Show with a line saying whether Export keeps it,
  Show in folder and Copy details. It stays open while stepping through photos.
- Approved by the owner with the shortcut I. A long folder path keeps its start and the
  photo's own folder and drops whole folders from its middle (drawn in the `-location` state).
  The owner added: the cut path shows whole in its tooltip, and a small copy button after it
  copies the photo's full path.

### 2026-11-06 (the owner back: About's icon, Batch without the grid)

- About cut its icon short at the bottom (the owner's screenshot): it took the same margin from
  every edge, but the icon's canvas has less below the lens, where its side is. It now cuts
  around the part the icon draws, checked at 1x and 2x.
- The owner dropped Batch's list or grid switch: the grid showed only names and pictures, none
  of the list's size, status or reason a photo was not saved, and it only appeared in Edit and
  export. Batch shows the list; the five Batch mocks lose the switch, and its glyph is gone.
- FEATURES S5 is done (folders, presets, looks and LUTs dropped anywhere, files from a file
  manager pasted).
- The owner's answers, in Waiting for the owner: both changes for review approved, the looks
  kept for now, white balance presets RAW only, the Batch folder stays "PixelFx", no push yet,
  the photo information panel next.
- State: 33 commits since `e4c0b91`, local only, none pushed. 42 app suites pass, in the
  normal build and under AddressSanitizer and UndefinedBehaviorSanitizer; 167 library tests;
  the linters clean.

### 2026-11-06 (testing every feature, the owner away)

- A test drives the whole window on odd photos (one pixel, a 4000 x 3 strip, a 2 x 3000 one,
  grey, transparent, 16 bits, a name with accents, CJK and "#", a broken and an empty file):
  every action, every slider to both ends, 80 undos and redos, every mode; it fails on any Qt
  warning (`tst_monkey`). It found a 0 px icon painted onto nothing in Batch (now no icon).
- Settings stopped at 492 px and scrolled with half the window free, cutting its last section;
  it is as tall as its page now (the scroll area every other sheet uses).
- Paste settings to several photos, after copying a photo with no edits, showed an empty list
  and a Paste that did nothing; it says there is nothing to paste and Paste is greyed out. Paste
  was also enabled at first with nothing ticked.
- A damaged photo was reported in libvips' words ("VipsJpeg: premature end of JPEG image"); the
  library's plain wording missed it by a capital letter and now matches any case.
- A form's label is centred on its field but not on a note under it (Photos at a time).
- The straighten dial under the photo drew white numbers and ticks whatever the canvas, so on
  the light theme's canvas, middle grey or white they all but vanished; they are dark on a
  light canvas now.
- The end-to-end test on real photos (every look and preset, crop, heal, every export format,
  batch, the command line) passes.
- Keyboard shortcuts listed every key that is bound, but not every bound key: New look
  (Ctrl+Shift+N), Quit (Ctrl+Q), Crop's Enter, Escape and X, and Heal's Delete were nowhere to
  be found. They are in the sheet and its mock now (with Close photo and Adjust a colour, which
  the app already showed); a test checks every key the sheet lists is bound. For the owner's
  look: the mock's rows changed.
- Preset, look and watermark names became file names through three copies of one function,
  which could cut an emoji in half at 80 characters and let through names Windows refuses (a
  trailing dot or space, CON, NUL, COM1...). One `core::fileSafeName` serves all three, tested.
  Export and Batch names use it too: a template such as ".{name}" no longer hides the photo,
  and a long name (a CJK one is three bytes a character) stays within what file systems allow.
- A save that failed (a folder without write permission, a missing folder, a full disk) said
  so in libvips' words; it says what is wrong in plain words now, tested in the library.
- A form's name stayed lit when its field was greyed out (On open with Remember edits off,
  Quality for PNG); names now grey out and come back with their fields, in every form.
- Batch lost photos: two photos of one name from different folders, saved into one folder,
  both became "photo-edit.jpg", the second over the first (with Overwrite or with two workers
  choosing at once). A run now keeps the names its photos took (`core::NameClaims`, shared by
  its workers): a name another photo of the run has gets a number, whatever If it exists says
  of the files from before. Compress only the same. Tested with sixteen photos, four at once.
  Exporting a selection of the filmstrip the same: with Overwrite, a second photo.jpg from
  another folder replaced the first.
- Every unnamed button was looked for: only the magnifier in the search fields had no name for
  a screen reader; one helper adds it, named "Search". Choose the recipe lacked that magnifier,
  which its mock shows, and was 980 px wide, the mock 900 (a sheet size of its own now).
- A text watermark with its text cleared was saved and used, printing nothing on every photo;
  it says to type the text first, as a logo watermark without a logo already did.
- Empty lists in the left panel, each looked at: Presets' Favorites and Recent showed a heading
  over nothing (the note was written but never shown); the Looks tab said nothing when a search
  matched no look or when there were no looks of your own. Each now says why, as Looks'
  Favorites did. Checked along the way: no leak over 120 photos opened and closed (memory
  levels off as the caches fill), a 96-megapixel photo opens in under 2 s, the fuzzers (document,
  loader, EXIF, .cube) found nothing in a minute each, the command line refuses bad input with
  a message and exit code 2, and a live theme switch matches a light start pixel for pixel.
- Batch on photos that cannot be read: one damaged file was "damaged or incomplete" in the
  list and "not a photo PixelFx can read" after the run (the loaders word it differently); the
  library now decides from the file's first bytes, the same everywhere, and an empty file says
  "The file is empty." The preview strip showed two empty boxes and a live View larger when no
  photo reads; it hides. The finish line no longer adds "0 bytes" when nothing was saved.
- Adjusting a colour on the photo (T) opened the Color mixer below the open groups, out of
  sight, so a drag changed sliders nobody could see; the mixer now comes to the top of the panel.
- Comments across the app cited mocks or the reason for a UI fix (the owner's rule: comments say
  what the code does); some 80 rewritten or dropped.
- Paste image (Ctrl+V): photos copied in a file manager open as themselves (they were ignored);
  with nothing usable on the clipboard it says so instead of doing nothing; two pastes in one
  second no longer share a file name (the second replaced the first).
- Dropping on the window (and files on the command line): a folder goes to Batch, as Open
  folder does (it was refused as an unknown file); presets, looks and .cube LUTs are imported;
  a file PixelFx does not open is said in a toast instead of replacing the photo on show with
  an error.
- A long file name in the header was cut off mid-letter when the window was narrow; it is
  shortened in the middle, keeping its start and extension, with the whole name in a tooltip.
- Remembered edits that could not be written (a data folder without write permission, a full
  disk) were only logged; the person is told once a session that edits are not being kept, and
  to export to keep them.
- Two other saves failed silently: Save as preset's copy to a file, and the watermark sheet's
  Save as; both say so now.
- Export with If it exists: Overwrite, the photo's own folder and a name of just {name} would
  write over the original photo without a word (Batch warned); Export shows the same warning
  line under the name now (one shared `warningLine`). For the owner's look: a line Batch
  already shows, new in the Export sheet.
- Every test, the one that drives the whole window on odd photos included, passes in the
  AddressSanitizer and UndefinedBehaviorSanitizer build with nothing reported; Auto on black,
  white and flat pictures stays in range; typing in a search field never fires a letter
  shortcut (V, C, R, B). Tests keep all three.

### 2026-11-06 (a new app icon)

- After a round of concepts here, the owner supplied the icon: a lens ring around six spectrum
  blades (`app/data/icons/hicolor/`, with its symbolic icon; Flathub's checks pass, 0 warn).
  Qt's SVG renderer ignores `<clipPath>`, which the blades used, so each blade is now the exact
  part inside the lens: the same picture, drawn right by Qt and KDE. Brand colours for the
  metainfo: #99c1f1 and #2a5a9e. Review: `DOCS/mocks/reviews/app-icon.md`.
- The window, About and the header's mark (approved by the owner 2026-11-06, shown in every
  mock's header) use it through `icons::brand()`, built in as `:/icons/pixelfx.svg`; the
  PixelFx 1 PNG and the painted colour wheel are gone.

### 2026-11-06 (the empty Batch page and Choose the recipe)

- The approved `batch-empty.html`: with no photos the list is a drop zone, Add photos and Add
  folder, in each mode's words ("Drop photos or folders to compress"), lit with "Drop to add
  them" while photos are dragged over the page; Clear, the list or grid switch and Process wait
  for photos.
- The approved `batch-recipe-picker.html`: Choose the recipe (`RecipePicker`) in place of the
  menu of every preset. Groups on the left (No look or preset, an opened recipe's own steps,
  All looks and Mine, Favorites, Recent, the user's groups, then the catalog's categories, with
  counts), a search through looks and presets by name and description, tiles of the batch's
  first photo with each, five to a row as the editor's (`tileGrid` takes a column count), the
  footer saying what the selected one does and its button naming it; double-click or Enter
  uses a tile. Then add preset opens the same sheet with presets only ("Add a preset on top").
  The group and the search are kept while the app runs.
- Plurals: the overwrite question's two counts had no singular; filled, and `tst_plurals` now
  fails when a count in the code has none.
- Debug: `PIXELFX_DEBUG_BATCH_STATE=picker` and `ontop` grab the page with the sheet open.
- Checks: 40 app suites (the drop zone in both modes, the picker's groups, search, choice and
  buttons, every count's singular), the library tests, the linters; grabs of the empty page in
  both modes and at 1024 by 640, and of both pickers.

### 2026-11-06 (the Batch audit)

- An audit of Batch against its mocks (a separate read-only pass) found 34 issues; the owner
  asked for all of them fixed, mocks for what has none, and the wording made one.
- Bugs: Batch asks before saving over originals ("Overwrite 3 photos") and says so under the
  name; every row says Waiting when a run starts, so nothing of an earlier run stays; Stop says
  "Stopped: 3 of 30 saved" and offers Process the rest; Compress only fails a file that is no
  photo instead of keeping it "unchanged", and its before and after is of the top saved photo
  of the sorted list once a run ends (Biggest saving sorts by the share saved, as the pills
  say); one Compress button, "Compress again" after a run; the list, its Clear, the rows' ×,
  Add and the mode hold still while a batch runs; an error reads as one (alert, danger colour,
  nothing of the last run beside it); the window title says "Saving 3 of 30" as the page does
  and "Batch" after; Convert and resize starts with nothing on top, and the card says what is
  on top when there is; a file that is no photo is said so when it joins the list; Settings
  from a photo needs a photo open; Keep edits needs Remember edits; the grid marks failures.
- Plurals: "1 photos" everywhere, as no translation was loaded; English plural forms now ship
  (ADR-014).
- Batch recipes version 6 carry the colour space (always written "srgb" before) and output
  sharpening; More... shows both, with Export's own controls.
- Alignment: sections spaced as the mocks (`sectionColumn`), the recipe source a segmented
  control, a small Sort list, Compress's choices as wide as their words, pills hugging their
  words, the row's × where the mock has it, scroll bars in the list's padding (the style's
  claim that they did was only a comment; `keepScrollBarInPadding` makes it so), the preview
  bar at its height from the start with pictures sharp on HiDPI, the recipe card's line on two
  lines, the brand mark at 22 px.
- Accessibility: focus rings on the borderless buttons (row ×, list or grid, links, segments)
  and 24 px targets for the × and links; light `success` from 4.1:1 to 5.3:1 on pills.
- Wording: photos in both modes; the quick jobs "Convert and resize" and "Compress photos" as
  their modes; Overwrite and Skip, Original, Change..., and 16 bits per channel as Export says
  them; JPEG XL and HEIC by name; sizes with a decimal below 10 MB; Resume with a play glyph. The
  mocks say the same.
- Left as they are: the 14 px panel padding and 18 px check boxes (app-wide; the mocks drift
  between 14 and 16), the header title's 8 px (the approved editor header), "Process" (not
  renamed "Save"), and the card's "Look, strength 72" against "Preset: ..." punctuation.
- Drawn, not built: `batch-empty.html` and `batch-recipe-picker.html`, waiting for the owner.
- Checks: 164 library tests (version 6), 38 app suites (overwrite, locks, waiting and the rest
  of a stopped run, Compress failing a damaged file, plurals, recipes keeping both settings),
  the linters; grabs of Batch ready, Compress done and the small window against the mocks, and
  of the welcome page, the editor and Settings.

### 2026-11-06 (the large preview against its mock)

- Fixed (owner's report): opened from the look editor, the large preview was the size of the
  look editor's sheet, not the window as `preview-viewer.html` draws; it now covers the app's
  window from wherever it opens. Before, Split, After sits at its own height in the bar (it was
  stretched to the bar's), Split has its icon, and the photo carries its "After" or "Before" tag
  (`Canvas::setTag`; the split keeps its two tags).
- A debug sheet, `lookpreview`, opens the large preview from a look editor.

### 2026-11-06 (Close photo)

- The approved `close-photo.html`: Close photo (Ctrl+W) in the main menu after Paste image, and
  in the Keyboard shortcuts sheet; a × on a filmstrip tile under the pointer (a 20 px circle in
  a 24 px target, "Close b.jpg" to screen readers) and a tile menu (Close photo, Close N photos
  for a selection, Close all photos). The next photo opens, or the one before when the last
  closed; after every photo the welcome page, the closed photos first under Recent. Greyed out
  in Crop and Heal and on Batch. `Document::close` and `Document::isEdited`; the strip keeps its
  pictures when photos leave it.
- Decided by the owner: with Remember edits off, closing an edited photo asks first ("Close
  without keeping the edit?", Close photo or Cancel, Cancel the default); drawn in the mock (7).
- Ctrl+W is set by its keys: `QKeySequence::Close` is Ctrl+F4 under some platform themes.
- A debug hook, `PIXELFX_DEBUG_CLOSE`, closes the photo on show once it opens.
- Checks: 38 app suites (the document closing and dropping an open on its way, an undone edit
  not counted as one; the tile's ×, its names and the menus), the linters; grabs of the menu,
  closing to the next photo and to the welcome page.

### 2026-11-06 (sheets that fit the window)

- Fixed (owner's report): Export on a photo whose shape differs from the size squeezed the
  framing card, its picture drawn over Crop to fill and Fit inside. Sheets sized themselves to
  their content once, when built, so a card shown later was squeezed, and Export with the card
  is taller than many windows. Every sheet except the editor sheets now follows its content's
  height as it changes, no taller than the window under it (and the screen), and stays centred.
  Export's form scrolls in a short window while its title and buttons stay, the scroll bar in
  the sheet's right padding (`SheetScroll`, `addScrollingContent`). The frame picture keeps its
  height and draws inside it; its arrow keys no longer depend on its size.
- The same squeeze showed in Save as preset (its rows overlapping); it now gets its height.
  Its "Black & white" part read "Black _white" ("&" taken as a shortcut mark): fixed.
- Checks: 38 app suites (the sheet growing with the card and shrinking back, the picture at
  full height, the part's name), the linters; grabs of Export on a portrait photo at three
  window sizes and of the other sheets, unchanged but Save as preset.

### 2026-11-06 (Use lens profile in Optics)

- The approved `editor-lens-profile.html`: Use lens profile at the top of Optics with the
  profile's name under it ("No profile for this lens yet: ..." or "The photo does not say which
  lens took it" when none is found), Distortion and Vignetting (0 to 200) while it is on and a
  profile is found, and a divider above Remove chromatic aberration. The rows are left out when
  no Lensfun database could be read. The profile is looked up on the worker that opens the
  photo. Optics, not Detail, now holds the lens profile settings in Save as preset, Paste and
  Batch ("Lens profile" by name).
- The catalog gains the two EmulsionPresets lens tools under Tools, "Lens profile, on" and
  "Lens profile, off" (owner, 2026-11-06). The importer now keeps `LensProfileEnable` and
  `AutoLateralCA` of 0 as an explicit off, as it does for colour against black and white, so a
  preset holding Lens Corrections turns them off; "Reset everything" and "Reset effects and
  detail" now reset them too, and "Dehaze" turns the lens profile on as its source does.
- Fixed: the catalog's file names dropped the sign, so Exposure +0.3 and -0.3 (and the 0.7 and
  1 pairs) wrote one file and three presets were lost; 431 ship now (426 before). The other
  files changed only their version (2 to 4, as `pfx import` writes now).
- Checks: 163 library tests (off kept off on import), 38 app suites (the row's states, the
  Optics part), the linters; offscreen grabs of the row on, off and without a profile against
  the mock on a scratch photo.

### 2026-11-06 (a review of the day's code)

- A second reading of everything since the RAW work found, and this fixes: Lensfun searches ran
  from several render threads at once while Lensfun switches the process's number locale (now
  one at a time, each camera and lens looked up once per database); a database swapped by
  `setLensDatabaseFolder` could be freed under a running render (now shared); a DNG's
  BaselineExposureOffset now goes only with the Camera rendering, whose tone it balances; a
  colour-space test compared two freshly made sRGB profiles byte for byte, which differ when the
  clock ticks between them (found by running the suite in parallel; it now compares names).
- Left, and written in ADR-013: a lens-centre offset on a photo shot turned. The owner's DNG
  has four GainMap opcodes, all 1.0: the phone already corrected its shading, so applying
  opcodes would change nothing there.
- Also: the look editor shows sizes as a share of the long edge ("0.50%").

### 2026-11-06 (`pfx`: looks and baking)

- `pfx apply --look FILE[:STRENGTH]` applies a `.pfxlook` at its own strength or the one given;
  `pfx bake LOOK OUT [SIZE]` writes a look's colour steps as a `.cube` and names the steps left
  out (FEATURES C7). Tried by hand on a scratch look (sepia, vignette, blur): applied at 80 and
  30, baked at 17 points, a non-look file and a bad size refused in plain words. Batch recipes
  stay with the app's `pixelfx --batch`: their watermark is drawn with Qt.

### 2026-11-06 (Favorites for looks)

- The Favorites chip of `editor.html`'s Looks tab, built as the preset catalog's: F on a tile or
  its menu (Add to Favorites, Remove from Favorites) stars a look, built-in or the user's, the
  chip lists the starred ones or says how to add one, and they are remembered
  (`looks/favorites`). The star takes the place of the Mine badge on a starred look of the
  user's; its section already says whose it is. The menu now opens on built-in looks too.
- Checks: 38 app suites (`tst_looks_panel`: F stars, the chip lists, the empty note), the
  linters; the panel grabbed headless with two looks starred.

### 2026-11-06 (creative steps for looks, FEATURES E9)

- PixelFx 1's creative operations as look steps, in a `creative` group (stages 91 to 98, after
  colour grading): Sepia, Colorize (hue, saturation), Threshold (level), Invert, Blur and
  Pixelate (sizes as shares of the long edge), Emboss and Edges (offsets as shares of the long
  edge, sub-pixel on small previews so they scale). Grayscale is the existing Black and white
  step; v1's Edge and Sobel became one Edges step. The keys join presets version 4 and recipes
  version 5 (nothing was written with those versions before today). Looks keep version 1: they
  already keep and skip steps a PixelFx does not know.
- The registry says which operations a look may use (`OpSpec::lookStep`): not geometry, heal or
  the lens profile, which today's lens work would otherwise have put in the look editor's Add
  step menu. Every step there now has a name (six showed their ids, such as
  `chromatic_aberration`).
- Checks: 161 library tests (each step's effect, a golden image and a scale test each, reviewed
  as a contact sheet; Edges brightened after that review), 38 app suites (Add step offers names
  only, never geometry, heal or the lens profile), the linters.
- Also: FEATURES statuses brought up to date with the code (34 rows were stale).

### 2026-11-06 (lens profiles, ADR-013)

- ADR-013 written as the owner asked: Lensfun 0.3.4 (LGPL-3.0, its database CC-BY-SA-3.0),
  optional at build time (`PFX_WITH_LENSFUN`), built in `scripts/build-deps.sh` for dev builds
  (pinned, SHA-256), a THIRD_PARTY row and its notice in About.
- libpixelfx: `pfx/lens.h` (`LensFacts` from Exif, `lensProfileName`, `lensProfilesAvailable`,
  `setLensDatabaseFolder`) and the `lens_profile` step at stage 2 (`detail.lensProfile.enable`,
  `distortion` and `vignetting` 0 to 200): vignetting undone, then distortion and the profile's
  lateral CA in one resampling, scaled to fill. The photo's camera and lens travel in
  `RenderOptions::lens`; the editor, thumbnails, Batch, Export and `pfx apply` pass them.
  Presets version 4, recipes and batch recipes version 5 (presets.md, tests). The Lightroom
  importer maps LensProfileEnable and its two scales.
- Checked on the system's database: Canon EF 24-70mm f/2.8L, Sony FE 24-105mm f/4 and Nikon
  AF-S 50mm f/1.8G are found from their Exif names; the Canon at 24 mm f/2.8 lifts its corners
  from 0.2 to 0.77. The owner's Galaxy S22 Ultra is not in the database (phones rarely are).
- Not built: the Optics control. `editor-lens-profile.html` and its review wait for the owner.
  The two EmulsionPresets lens tools left out of the catalog (ADR-009) can join it once that
  control exists.
- Checks: 154 library tests (a lens database written by the test: lookups, vignetting and its
  amount, distortion at two sizes, Exif facts; version bumps; the importer), 38 app suites, the
  linters.

### 2026-11-06 (Export: sharpening, colour space, size estimate)

- FEATURES X3, X4 and X7 from the approved `export.html`. libpixelfx: `sharpenForOutput` (an
  unsharp mask on perceptual luminance in output pixels, Screen at 0.5 px and Print at 1 px, its
  halos softened), `SaveOptions::space` with Display P3 and Adobe RGB profiles made by LittleCMS
  and the photo's own profile kept as stored for Same as original (`LoadedImage::profile`;
  anything not RGB saves sRGB), `encode` to memory and `fromSrgb8`.
- The app: Sharpen for and Color space rows under Size, remembered with the other choices; the
  note says "About 1.9 MB." a moment after any change. The estimate encodes the preview at up to
  1 megapixel and carries it to the output size with a measured trend (bytes a pixel as
  pixels^-0.30 lossy, ^-0.24 lossless); on four photos of 1.5 to 48 megapixels it lands within
  about 30 percent of the written file for JPEG and PNG (the test photo: 1.9 MB estimated, 1.6 MB
  written); WebP can read up to 60 percent high, AVIF, HEIC and JPEG XL are not measured yet.
- Batch keeps writing sRGB without output sharpening; its recipe format is unchanged.
- Also: a DNG's BaselineExposureOffset is added to its baseline exposure (the tag was read but
  unused; the owner's file has none).
- Checks: 152 library tests (colour spaces round trip and keep what sRGB clips, the original
  profile kept byte for byte, encode equals save, output sharpening, 8-bit sRGB round trip), 38
  app suites (`tst_exporter`: profiles embedded, the estimate near the file; `tst_export_sheet`,
  `tst_export_options`, `tst_settings`), the linters; the sheet grabbed headless.

### 2026-11-06 (Camera RAW)

- The approved `editor-raw.html`, built (ADR-012). libpixelfx reads linear DNG itself (the owner's
  Galaxy S22 Ultra file: DNG 1.7, one JPEG XL strip of 4000 by 3000, decoded by libvips in 0.3 s),
  through the DNG default rendering: levels, colour and forward matrices with calibration
  interpolated for the white, the as-shot neutral, baseline exposure, crop and orientation, the
  profile gain table map for Camera (flat in that file); a clean Exif carries the camera. Other
  RAWs go to LibRaw as before. `color.profile` gained `camera`: presets version 3, recipes and
  batch recipes version 4 (presets.md, tests).
- The app: a RAW shows its JPEG at once with "Developing RAW", opens on Camera, develops again when
  the profile leaves or returns to Camera (the file's tone), shows Temperature in Kelvin around its
  as-shot white with Daylight to Flash; thumbnails of unedited RAWs use the JPEG inside; export
  develops with the right tone; a RAW that cannot be read offers "Open its preview instead".
- Changed while building: Kelvin's reach is the engine's (as-shot white halved to doubled), see
  `reviews/editor-raw.md`.
- Checks: 148 library tests (a written DNG, uncompressed and JPEG XL: levels, colour, exposure,
  orientation, Exif, the gain table map; a RAW's preview), 38 app suites (`tst_raw`), the linters;
  the owner's DNG (a copy in the scratchpad) opened headless while developing and after.

### 2026-11-06 (batch notification)

- FEATURES S6 on Linux (ADR-011, owner's approval of Qt D-Bus): `services::Notifier` notifies
  when a batch ends and the window is not active, through the portal in Flatpak and
  org.freedesktop.Notifications elsewhere; clicking it brings PixelFx forward on Batch. Not sent
  for real in this session (it would show on the owner's desktop); the call is the one the
  freedesktop specification gives.

### 2026-11-06 (Online guide, live theme)

- Online guide in the main menu and About, to the GitHub wiki; the web addresses are in one place
  (`core/links.h`).
- The theme switches at once from Settings, and System follows the desktop while PixelFx runs.
  Icons draw through a `GlyphEngine` that keeps the token it was made with and paints in that
  token's colour now; labels show glyphs through `icons::setGlyph`, redrawn by `switchTheme`;
  the palette and style sheet are applied again; painted widgets read the tokens as they paint
  (the curve editor's colour now too). `tst_theme` checks an icon, a glyph and the palette, and
  `PIXELFX_DEBUG_THEME` grabs a live switch.

### 2026-11-06 (Batch: the views as their mocks)

- Owner: Batch had inconsistencies and misalignments with its mocks. Compared `batch.html`,
  `batch-running.html` and `batch-done.html` with headless grabs (the Batch hook gained
  `PIXELFX_DEBUG_BATCH_STATE=ready|running`). Fixed: both lists now build their rows with one
  `makeFileRow` (ListRow: hover, the mock's padding and edge, a picture with rounded corners, the
  name and its line together in the middle); a failed photo's reason replaces its size line in
  the danger colour on both lists (`ListRow::setSubtitleError`); rows are exactly as tall as their
  content; the Compress list no longer leaves its empty-list note behind the first row; an
  unreadable file's picture has a placeholder.
- Left as is: the mock's per-photo percentage while saving ("62%") needs progress from inside a
  render, which the engine does not report; the row says "Saving".

### 2026-11-06 (Export: sizes as the mock)

- Owner: the Export sheet's size did not match `export-sizes.html`. Built what was missing: the
  grouped size list (`sizelist`, a delegate shared with Batch: headings, the pixel size on the
  right, a tick), the framing card (`FramePicker`: the photo dimmed, the kept part in a white
  frame dragged with the mouse or the arrow keys; `pfx::fillSize` keeps any part, not only the
  middle), Fit inside's border black or white by the photo, Custom size (width, height, dpi,
  kept under Mine), 300 dpi for the print sizes (`pfx::SaveOptions::dpi`). One factory,
  `MainWindow::newExportSheet`, sets the sheet up for both the window and the debug hook.
- Checks: 145 library tests (the frame at the left edge keeps the left), 36 app suites (the
  list's headings, Mine and Custom size, the frame's keys), the linters; the sheet and the open
  list checked headless against the mock.

### 2026-11-06 (owner's answers)

- Camera RAW mock (`editor-raw.html`) approved as described, Kelvin for RAW only. Lensfun: write
  the ADR and build. Batch finished notification: Qt D-Bus on Linux. Export framing under the Size
  row. Theme: switch live. Batch's subfolder stays "PixelFx". Online guide: the GitHub wiki.
  Sidecars next to the photo: later.

### 2026-11-06 (Batch: convert, resize and rename)

- The approved `batch-convert-rename.html`, built (FEATURES M8, M9, M12). The welcome page has
  Quick jobs: Convert or resize (Batch with "Convert and resize only", the renamed first recipe
  choice) and Make files smaller (Compress only).
- `NameField` serves both Batch modes: the template, + Token with an example from the first photo,
  the first three names as they will be saved (numbers added as the batch adds them), and a
  warning line when every photo would get the same name. New tokens {n} (padded to the count),
  {taken} and {camera} (from Exif: `pfx::readMetadata` reads it from the header), through
  `core::NameFacts`; the runner passes each photo's number. A `warning` colour token joined the
  app's tokens (DESIGN section 1 already had it).
- Settings, Performance: Photos at a time (Automatic, 1, 2, 4, 8); the runner takes it from the
  next run.
- Checks: 145 library tests, 36 app suites (new: tokens and the saved size in
  `tst_export_options`, the name preview in `tst_batch_compress`), the linters; the welcome page,
  both name previews and the Settings sheet checked headless against the mock.

### 2026-11-06 (Batch: Compress only)

- The approved `batch-compress.html`, built (FEATURES M10, M11, part of M12). libpixelfx gained
  `pfx/compress.h` and `pfx compress`: lossless JPEG through libjpeg-turbo's coefficient API,
  our own PNG writer (smallest colour type and bit depth, filters, palettes, every chunk kept in
  its place), our own palette quantiser with pngquant's quality scale (owner's choice over
  quantizr, which needs Rust in every build), our own GIF writer, TIFF and lossless WebP stored
  again without loss, location and previews removed from Exif, XMP, Photoshop and WebP chunks.
  ADR-010; THIRD_PARTY gained libjpeg-turbo and zlib (already in every runtime).
- Batch has "1. What to do" at the top: Edit and export (sections now 2 to 5) or Compress only
  (`CompressPanel`, `CompressList`, `CompressCompare`; `StatusPill` now shared by both lists).
  The runner compresses up to 4 files at once, replaces what the last run of the same list
  wrote, and keeps the original's most detailed spot when the originals are replaced. The mode
  and the choices are remembered, Replace the originals is not.
- Changed while building, recorded in the mock and `reviews/batch-compress.md`: the "Others"
  line now says only what is done (no AVIF encoder; a lossy WebP is never encoded again), the
  row's ⋯ became ×, a JPEG quality slider for Target quality.
- On copies of the e2e photos: JPEGs 6 to 14% smaller without loss in about 0.1 s each, a 3.7 MB
  PNG photo 1.9 MB lossless or 1.0 MB with fewer colours (quality 96).
- Owner's DNG from Samsung Expert RAW (Galaxy S22 Ultra) did not open: it is DNG 1.7, linear
  (already demosaiced) and JPEG XL compressed, which LibRaw reads only with Adobe's DNG SDK. The
  message is now plain words. For FEATURES O3: decode the JPEG XL tiles with libjxl (already a
  dependency), then black and white levels, AsShotNeutral, ColorMatrix1, BaselineExposure (+3
  in that file) and its ProfileGainTableMap (Samsung's local tone map). The file also holds a
  full-size JPEG preview.
- Checks: 145 library tests (new: every lossless method decodes to identical pixels), 36 app
  suites (new: `tst_batch_compress`, Compress only in `tst_batch`), the linters; the real app
  headless on six mixed files matches the mock.

### 2026-11-05 (a slow preview)

- The approved slow preview of `editor-zoom-loading.html`: the Preview tag and the line come
  with the previewed picture, not when it is asked for, so the photo never claims a preview it
  does not show yet. `Document::HoverState` (Off, Preparing, Shown) reports it; past 0.3 s the
  chip says "Preparing preview", zoomed in or not, then "Updating" while the sharp picture of the
  preview renders. A render made for a tile already left is dropped, and so is one for a preview
  already ended.
- Owner: the ring always turns. `BusyRing` no longer stands still when the desktop turns
  animations off, and `animationsWanted` went with it (nothing else used it).
- Build: the host's Qt went to 6.12 and the dev build broke, because CMake had been taking
  moc, rcc and uic from the host (it searches the SDK's `lib/x86_64-linux-gnu` only on a
  Debian-style host). `scripts/dev-build.sh` now finds Qt's tool packages in the SDK and drops
  any found on the host before; the snap and Flatpak builds are not affected.
- Checks: 35 app suites, 134 library tests, the linters; the real app headless with a 6021 by
  3422 photo shows "Preparing preview" and no tag until the preview arrives, as in the mock.

### 2026-11-05 (Looks, hover preview)

- The approved `editor-looks-preview.html`: resting on a look previews it as presets do (the look
  at its own strength over the edit, in place of the applied one; Original as no look), and
  "Applied" above Look strength names the look the slider belongs to ("None" greys it).
- One `HoverPreview` now runs the wait, the bar, the quicker next tile and the moment's grace on
  leaving for both tabs; `MainWindow::previewOnPhoto` shows either on the photo (the Preview tag,
  the line, Escape), so there is one copy of the behaviour.

### 2026-11-05 (zoom: loading the sharp picture)

- The approved `editor-zoom-loading.html`: zoomed in past the preview, a chip beside the zoom bar
  says "Loading" while the photo is read at full size and "Updating" while an edit is redrawn,
  once the wait passes 0.3 s; the bar itself never moves. `Document::FullState` reports where the
  full-size picture is; `BusyRing` turns (it stood still when the desktop wanted no animation until the
  owner asked for it always on, below); screen readers hear it as an announcement. The full-screen viewer shows
  the chip when going to the next photo.
- Owner's finding: zoomed in, a preset's preview did not show (only the small preview and the
  histogram changed, the sharp picture of the edit stayed on top). A preview now comes after a
  deliberate wait, so it is drawn at full size too; when it ends, the edit's sharp picture is
  kept and comes straight back.

### 2026-11-05 (hover preview, Collapse all)

- Owner's findings: opening a category in full showed a count of 0 and no pictures past the
  first six (new tiles of a shown panel count as hidden until the next event loop pass, so the
  panel now keeps its own list of the tiles it built); a preset previewed the instant the pointer
  touched it; the notices list's highlights overlapped (rows were laid out before the style sheet
  padded them; every item view now lays its rows out again once styled).
- The approved hover preview: 0.35 s resting on a tile, 0.15 s once a preview shows, a bar
  filling along the bottom of the tile's picture meanwhile; a Preview tag on the photo and the
  line "Preview of ..." at the top (`paintCanvasBanner`, shared with the colour picker's line);
  Escape, a click, or leaving the tiles ends it; keyboard focus previews as hovering does.
- The approved Collapse all: a button at the end of the adjust panel's top row and in the
  Presets search row; a heading's menu with Collapse all, Expand all, One group open at a time
  (remembered) and Open only <group>; Alt+click on a heading opens only that group.

### 2026-11-05 (a colour picked on the photo)

- The approved `editor-mixer-target.html`: the target at the end of the Color mixer's swatches
  (T, and in the Photo menu) picks the colour under the pointer and a vertical drag changes it in
  the mode on show, or in the B&W mix; a colour between two bands moves both by their shares,
  the panel names the second ("Also moves Aquas hue +7"), one drag is one history step. The
  colour is read before the mixer (`pfx::settingsBefore`, `Document::colorBeforeMixer`, a 640 px
  render cached until an earlier setting changes) and its bands come from the library
  (`pfx::hueBands`), as the mixer weighs them. Escape, T, Crop, Heal and Batch stop it.
- The CHANGELOG now tells today's work: Lightroom presets in full, the catalog, black and white,
  colour grading in four ranges, Texture, Regions, Optics and Calibration.

### 2026-11-05 (the preset catalog)

- The Presets tab of the approved `editor-presets-catalog.html`: search, All, Favorites (F),
  Mine and Recent, the user's groups, then Essentials and the twelve catalog categories as tiles
  of the photo, six per open category with "All N" for the whole of one, categories closed by the
  user remembered, a tile's menu (Apply, Apply to the selection, Favorites, Copy to My presets,
  Export), and the Amount of the preset just applied (0 to 200, as Lightroom's): it blends from
  the photo before the preset, a setting moved by hand since keeps the user's value, and undo
  takes it away with its step. Only the tiles on show get their picture rendered.
- The catalog: 429 of EmulsionPresets' 451 (431 since 2026-11-06) (MIT, ADR-009), converted by `scripts/catalog/
  build_catalog.py` with the new `pfx import`, renamed without brands or people
  (`scripts/catalog/catalog.tsv`, a test checks it), described from their settings
  (`core::describeSettings`). On the 33 ColorWall photos every one renders and, outside Tools,
  visibly changes the photo.
- Shared pieces: `PhotoTile` and `tileGrid` for the Looks and Presets tabs, `core::hueName`.
- Decided by the owner: the names are fine; Tools stays, last in the list.

### 2026-11-05 (Lightroom compatibility, the panel)

- The panel of the approved `editor-develop.html`: Color or Black & white at the top of Color
  (V, also in the Photo menu; off while healing, where V shows the spots), Profile, the Color
  mixer turning into the B&W mix, Color grading with four small wheels choosing the range and
  one large wheel editing it, Curves with Points and Regions (split marks dragged or moved with
  the keys), Texture, a "More" per group remembered between sessions, and the new Optics and
  Calibration groups. Black & white, Optics and Calibration are parts of presets, pastes and
  batch recipes, and a colour preset now turns a black-and-white photo back to colour.
- Shared pieces: groups are built from a table of controls (sliders, choices, checks, with
  sub-headings and More), `ChoiceRow` and `CheckRow`, `GradingRangeButton` drawing the same wheel
  as `ColorWheel` (`paintColorWheel`), `RegionCurveView` drawing the library's own sampler, and
  `SegmentFit` for segmented controls that share their row.
- Small fixes on the way: a "&" in a button's label no longer becomes a shortcut letter; values
  that cannot go below zero show no plus sign; every coloured track has a rim; disabled slider
  rows look disabled; the colour mixer stands aside in black and white, as in Lightroom.
- Next: the preset catalog (`editor-presets-catalog.html`) and the curation of the 451 presets.

### 2026-11-05 (Lightroom compatibility, library)

- Decided by the owner: PixelFx reads and renders every global develop setting a Lightroom
  preset uses (FEATURES P8), each shown as a control; secondary sliders sit under a "More"
  disclosure in their group; colour grading gets midtones and global (the approved grading mock
  is reopened); lens profiles wait for their own ADR (lensfun). A large built-in preset catalog
  follows (P9), browsed as tiles with an Amount slider (mock first).
- Measured on EmulsionPresets (451 MIT-licensed Lightroom presets, 103 settings): before, 413
  presets lost a setting on import, the 83 black-and-white ones came in as colour. Now 449 import
  with nothing skipped; the other 2 switch on a lens profile.
- libpixelfx: black and white with the B&W mix; colour grading with midtones, global, a
  luminance per range and blending; Texture; Calibration (primaries, shadow tint); the vivid
  profile; the parametric curve with its splits; sharpen detail; noise reduction detail,
  contrast and smoothness; vignette feather, style and highlights; grain roughness; lateral
  chromatic aberration measured on the photo; defringe. Every default renders as before (the
  golden images are unchanged); each new setting has a golden image, a scale test and a
  behaviour test. The importer maps all of them, profiles included.
- Formats: presets version 2, recipes and batch recipes version 3, older files read unchanged.
- Next: the app's preset parts for B&W and Calibration (they change the Save as preset and Paste
  sheets), the controls mock, then the catalog mock.

### 2026-11-05 (UI against the mocks)

- Decided by the owner, then changed the same day: sheets have no system title bar. They are
  drawn as the mocks draw them (rounded, bordered, over a dimmed window) and moved by dragging
  them; the system's own dialogs (file and colour pickers, short questions) stay native.

- Sheets against their mocks, one at a time: Export, Watermark, Save as preset, Paste settings,
  Settings, Keyboard shortcuts, About, What's new and the look editor. Shared pieces came out of
  it: `Sheet` (frameless, over a dimmed window), `PartChecklist` (the parts of a preset or a
  paste with what changed), `StepList` (reorderable steps), `ElidedLabel`, `setLabelColumn`
  (muted form labels in one column), `coverThumbnail`, `icons::toggle`. Sliders anywhere are
  `SliderRow`s, so every value can be typed and shows in the accent when changed.
- Left for later, from the mocks (drift list, section B): Export's Sharpen for, Color space and
  size estimate; Paste's Replace their settings; Settings' other pages; the watermark font
  weight. The mocks are drawn at 13 px; PixelFx uses the system font, which reads near 12 px
  on the owner's desktop.
- The owner found the Batch page far from `batch.html` and raw Fusion controls everywhere.
  Every screen was grabbed and set beside its mock; the gaps were shared, so they were fixed
  at the root: the style sheet now draws every standard control as `mock.css` does
  (checkboxes, radio buttons, switches, combo boxes and their lists, spin boxes, scroll bars,
  tooltips, progress bars, disabled states), from glyphs made in the theme's colours at start;
  every sheet has the mocks' title row with a close button and a footer bar
  (`sheetLayout`, `sheetFootBar`, `sheetCloseButton`); switches where the mocks have them
  (Settings, Watermark, Heal); the rail has the settings gear and the header its separators.
- Batch rebuilt from `batch.html`, `batch-running.html` and `batch-done.html`: the header
  shows "Batch, 6 photos · 129 MB" with Add photos and Add folder; the list has its title row,
  a list or grid view and Clear; status pills (Saved, Saving, Waiting, Not saved); the preview
  and the running or done bar with time left or taken and the size written; the recipe as a
  card with Change (presets and looks) or Settings from a photo, Then add preset, Never
  enlarge, "JPEG, quality 90" with More... (quality, bit depth, metadata, if it exists), the
  folder as ~/..., the name with the new {preset} token, Save as batch recipe and Process at
  the foot. Open batch recipe moved to the main menu (and a .pfxbatch can be dropped on the
  page). Fixed on the way: the recipe box showed blank (its separator was selected).
- Still different from the mocks, next: Export (AVIF and More in the formats, Sharpen for,
  Color space, the size estimate, quality labels above the slider), Save as preset (the
  photo's thumbnail, the Include card in two columns with counts), the look editor's step
  rows (handle, icon, menu), Settings' other pages, the presets panel rows.

### 2026-11-05 (spot heal)

- Spot heal (E20, mock `heal.html`), approved with its format by the owner today.
- libpixelfx: a `Spots` settings value and the `heal` operation at stage 5, before geometry, so
  spots are placed on the upright photo and a later crop never moves them. `healSpot` copies
  the source in, adds the difference between the two surroundings fitted as a plane per channel
  (a spot on a sky gradient matches both sides) and feathers it; `findHealSource` tries 96
  places around the spot, keeps those inside the photo and clear of the spot, and picks the one
  whose surroundings match and whose inside looks like the spot's surroundings (so another
  blemish is not copied in). Recipes are at version 2 for `heal.spots` (presets.md sections 2,
  4, 5). Tests: a blemish on a gradient, a stroke over a line, a second blemish avoided, the
  photo's edge, render with a crop, the format, and 200 random spots on tiny and thin images;
  clean under ASan and UBSan; the document fuzzer ran 4.7 million inputs with a spots seed.
- The app: Heal on the rail (R) with its panel (Spot or Brush, size, feather, opacity, Show
  spots V, Reveal dust D, the list with Remove all, Reset and Done), the canvas draws each spot
  (white with a dark outline, the selected one with its hollow source and a dashed line), click
  to heal, drag to brush, drag a spot or its source, [ and ] for size, Delete, the arrow keys to
  nudge, Escape to leave. Each spot is a history step and undo works in Heal. Copy and paste
  never carry spots. A `CanvasTool` hook keeps the canvas free of Heal's logic; Heal's part of
  the window is in `main_window_heal.cpp`. 33 app suites and 114 library tests pass.

### 2026-11-05 (batch format version 2)

- The owner approved batch format version 2 and the spots format for heal.
- Batch recipes (`.pfxbatch`) are at version 2 (presets.md section 4): the user's looks the
  stack refers to are embedded once each, a look on its own is a look step, a named size of the
  Export sheet can be the resize, and the watermark travels whole, its logo as base64. Opening
  a recipe (Batch page or `pixelfx --batch`) adds its looks to the user's looks when their id is
  new and its watermark unless an identical one is there (`core::adoptBatchResources`). Version 1
  files read unchanged, version 3 is refused as newer; library tests for both, app tests for the
  embedding, the adoption (twice, nothing doubled), a broken watermark and the page's round trip.
- The Batch page's Resize lists the Export sheet's named sizes, as `batch.html` shows ("Web ·
  2048 long edge"). This also fixes a bug: a size preset last used in Export silently decided
  the batch's size whatever Resize showed.
- Checked by hand: a version 2 recipe with a look and a centred text watermark, run with
  `pixelfx --batch` in a scratch profile, wrote a 1080 x 1080 square with the mark and added the
  look and the watermark. 32 app suites and 107 library tests pass.

### 2026-11-05 (mocks approved: grading, About, What's new)

- The owner approved `editor-grading.html`, `about.html` and `whats-new.html`.
- Color grading (E8) in the adjust panel, after the Color mixer: `ColorWheel` for the shadows
  and the highlights (angle the hue clockwise from red at the top, distance the amount, the
  values written under each wheel with the amount in the accent when set), the rim and the
  two-edged puck of the review, keys (Left and Right the hue, Up and Down the amount, Shift for
  10, Backspace resets; a held key is one history step), screen-reader text, Balance, the group
  dot and reset. `PIXELFX_DEBUG_EXPAND` opens a group for grabs.
- About (S11): the version and build, What's new, Source code and Report a bug, the credit
  line; Open-source notices with every shipped component and its licence text, copied verbatim
  from each project into `app/src/resources/notices` and built in (a test keeps the list in
  step with THIRD_PARTY.md); Diagnostics with versions (libpixelfx gained `engineVersions`),
  the system, the formats this build reads and writes, threads, Copy and Open the log folder
  (a test checks it holds no home folder, user name or licensing words).
- What's new (S8): the running version's section of the bundled changelog, the first five
  items and then "The full list of changes", a Version list once there are two releases, and
  "Show this after each update". It opens once after an update (500 ms after the window, never
  with `--batch`), and from the main menu and About. The changelog's lead is now two short
  paragraphs and its first five items are the mock's highlights.
- 31 app suites and 105 library tests pass; clang-tidy clean on the sources.

### 2026-11-05 (M3, part 1)

- Live editing: `RenderScheduler` renders previews on a worker, one at a time, with only the
  newest request waiting; `Document` keeps the decoded source, the settings and their history,
  with `setSettings` (live), `commit` (a step), `apply` (a preset as one step), `undo`, `redo`;
  `core::History` holds named settings snapshots (200 steps).
- The adjust panel from `editor.html`: Light, Color and Detail groups built from the registry,
  changed values in the accent, group dots and reset, double-click resets a slider, Reset all
  (Ctrl+R), meaningful tracks for temperature, tint and saturation. Undo, Redo and hold Space
  (or the eye button) for the original in the header.
- Verified: 12 suites pass against the snap runtime (new: history, adjust panel, document
  editing including 30 quick changes rendering fewer times with the last one winning); clang-tidy
  clean; a grab with the example preset applied matches the mock.
- Found and fixed: opening rendered the photo twice; changed values were not in the accent.
- CI: Linux and macOS green after the fixes; Windows still building its vcpkg packages.
- Export: `ExportSheet` from `export.html` (formats the build can write, quality, 16 bits for
  PNG and TIFF, original size or a long edge, metadata on or off, folder, file-name tokens,
  if-exists policy), `Exporter` (full resolution on a worker), a toast with the result,
  Ctrl+Shift+E; choices remembered in settings; `core::exportTarget` never picks the original by
  accident. libpixelfx gained `fitLongEdge` (Lanczos 3 in linear light) and `writableExtensions`.
  15 app suites and 43 library tests pass; clang-tidy clean.
- Histogram at the top of the adjust panel, computed with each render on the worker; 16 suites pass.
- Edits remembered per photo (`core::SidecarStore`): a recipe file per photo in the app's data
  folder, named by a fingerprint of the file (size and the first and last 64 KiB), read on the
  open worker, written in order on a one-thread pool after a short pause, flushed on close and
  before the next photo opens; no settings, no file. The library's recipe documents carry the
  `source` block of `presets.md`. 17 app suites and 44 library tests pass.
- Curves group (`CurveEditor`): RGB, R, G, B; click to add, drag, double-click to remove; drawn
  with the library's own sampler (`pfx::sampleCurve`) so the drawing is what renders.
- History popup (H) that jumps to any step; before and after split (\\) with a draggable divider.
- Auto (`pfx::autoAdjust`): exposure toward middle grey, blacks and whites stretched, a little
  contrast and vibrance when needed, a half-strength grey-world white balance for strong casts;
  the Auto button keeps curves and detail; `pfx apply --auto`. 48 library tests pass.
- CI Windows: vcpkg and Qt install; the build compiles with MSVC after one fix (an iterator
  spelled as a pointer). 13 of 17 suites passed on the first Windows run; the end-to-end
  single-instance test passed there. The four failures were test assumptions, now fixed: Unix
  paths in the command line and export tests, a /proc path in the log sink test, and a
  single-instance test sending from the primary's own thread (on Windows a local socket is a
  named pipe, which needs the other side to accept).
- Geometry in libpixelfx (`geometry` op, stage 10): quarter turns, flips, straightening that
  crops to the largest upright rectangle (2 px inside, so no soft corner shows), crop as
  fractions of the straightened photo; `pfx::outputSize` gives the size without rendering. A
  positive angle turns the photo clockwise (checked on a render). 53 library tests pass.
- Crop mode from `crop.html` (C): a rail with Edit and Crop; the crop box over the whole
  straightened photo with the rest dimmed, corner handles, Thirds, Grid, Golden and Diagonal
  guides; ratio chips (Free, Original, 1:1, 4:5, 5:7, 3:2, 16:9, 9:16), Landscape and Portrait
  (X), straighten slider in tenths of a degree, turn and flip buttons; Enter applies as one
  history step, Escape cancels. `core::turn` and `core::flip` act on the photo as shown (a turn
  under one flip goes the other way in the settings; a flip negates the angle and mirrors the
  crop). While cropping the preview renders without the crop, and moving the box renders
  nothing. Before and after now uses an original with the same geometry, so the halves line up.
  Not yet: the straighten dial under the photo, the level line, Auto straighten, Custom ratio
  and Perspective (the library has no perspective op yet). 19 app suites pass.
- Effects (vignette, grain, fade), Color mixer (eight bands) and colour grading (split toning)
  in the library; Effects and Color mixer groups in the panel from `editor.html`. Colour
  grading has no approved panel yet: `mocks/editor-grading.html` and its review are waiting for
  the owner (two questions in the review: a midtones wheel, own group or inside Color).
- The app opens what the libvips build reads (`pfx::readableExtensions`); dev builds now build
  libheif with libde265 and dav1d, libjxl 0.12 and LibRaw 0.22 (ADR-008 update), so HEIC,
  AVIF, JPEG XL and camera RAW open in dev runs. The welcome page shows Recent photos with
  their remembered edits.
- Speed: a 24 MP JPEG with a typical edit (light, white balance, curves, clarity, sharpen,
  vignette) loads, renders and saves in 1.1 s, down from 3.4 s (M3 target 1.5 s, met on a
  14-thread machine); peak memory 0.98 GB, down from 1.33 GB. Measured with `pfx apply` and
  per-step timing. Bounded memory (tiled rendering) is still open.
- CI: Windows (MSVC, vcpkg) green for the first time; macOS failed on a test that used
  QTest::mouseMove (it moves the real cursor there), fixed.
- Dehaze (dark channel prior, guided filter, haze map worked out at about 1000 px and applied
  at full size: 0.75 s and 0.69 GB at 24 MP instead of 2.35 s and 2.1 GB) in the library and
  the Detail group.
- Fuzzing (`make fuzz`): libFuzzer with ASan and UBSan. The document parser ran 8.5 million
  inputs clean after two fixes it found (a curve clamped into colliding points was kept broken;
  empty or dotted names and value-group collisions did not read back the same). The loader
  target runs about 2 inputs a second (thread start-up under the sanitizers), 600 runs clean;
  reading it showed an unbounded allocation from a hostile header, now refused over 512 MP.
- Zoom and pan from the zoom bar of `editor.html`: wheel zoom around the cursor, drag to pan,
  double-click for Fit and 100%, Ctrl+0, Ctrl+1, Ctrl+Plus and Ctrl+Minus, full screen; Crop
  mode always fits. Past the preview's resolution the Document decodes the full photo once and
  renders it 250 ms after edits settle (`setFullResolution`, `fullReady`); it is dropped when
  the view goes back. E switches to Edit mode. FEATURES has no row for zoom yet.
- Crop mode completed against `crop.html`: the straighten dial under the photo, Draw a level
  line (horizontal or upright, whichever is nearer), the Custom ratio chip, and the Perspective
  sliders (keystone in the library that never shows empty corners; turns and flips carry it).
  The four-corner fix and Auto straighten are left for later, as the mock allows.
- Color group: the White balance menu (As shot, Auto, Custom) and the picker (W), from
  `pfx::neutralWhiteBalance`, which inverts the white balance model. The mock's Kelvin presets
  (Daylight, Cloudy ...) are left out: on a JPEG they would be guesses; they come with RAW.
- Copy and paste settings (Ctrl+Shift+C, Ctrl+Shift+V) as a preset document on the clipboard,
  crop and turns excepted; paste a picture from the clipboard (Ctrl+V).
- Export: Keep location (off by default) strips GPS from Exif (unlinked and zeroed) and XMP
  without a metadata library; only formats with a working encoder are offered (AVIF and HEIC
  were offered and failed when libheif had no encoder).
- Settings sheet (`settings.html`, General page): theme (applied at start, since icons are
  tinted when made), canvas background, remember edits, on open, clipping warnings (J). The
  other pages and rows (monitor profile, GPU, batch threads, sidecars next to the photo) come
  with their features. The light theme was checked on a grab; the histogram now multiplies its
  channels on a light well.
- Presets (M4 started early, mocks `editor-presets.html` and `preset-save.html`): the left panel
  with the Presets tab (Looks waits for looks), grouped rows with pictures of the photo, search,
  import, the row menu, hover preview through `Document::setHoverSettings`; Save as preset with
  the units of the mock (White balance and Perspective apart from Color and Crop). A preset
  writes every value of each unit it holds, defaults included, so applying replaces exactly
  those units and presets stack. Built-in presets are a first draft made from today's
  operations (owner to tune).
- CI: Windows ran the full suite for the first time: two library tests assumed loaders the
  vcpkg libvips lacks (GIF, PPM); fixed in the tests. macOS: the curve test now double-clicks
  where the point is drawn.
- Lightroom and Camera Raw presets: `pfx::importXmpPreset` maps the crs: settings PixelFx has
  (the table of presets.md section 6) and lists the others; the Presets panel imports `.xmp` as
  "Lightroom: name" and says what was left out. Tried on hand-made presets only: real exports
  from Lightroom are worth a try.
- Filmstrip from the strip of `editor.html` and `apply-to-selected.html`: many photos open at
  once, pictures of their edits with the edited badge, the + tile, "1 of 4" or "N selected ·
  Select all · Clear", Left and Right (in the window's key handling so a focused slider keeps its
  arrows), Ctrl and Shift click, Ctrl+A. With a selection, presets and pasted settings go to every
  selected photo: the current one as a history step, the others through their remembered edits
  (needs Remember edits on), and the Presets panel says "Applies to N photos". Export with a
  selection is "Export N photos": the same sheet, each photo with its own settings, one after
  another. Paste with a selection asks which parts (Paste to N photos).
- Open: the curve editor's mouse test crashes (SIGSEGV) on the macOS CI runner only, after the
  synthetic double click; it is clean under ASan and UBSan on Linux (debug build, `build/asan`)
  and passes on Windows. Skipped on macOS for now; worth a look on a Mac.
- Batch (M5 started, mocks `batch.html`, `batch-running.html`, `batch-done.html`): the Batch
  mode on the rail (B) with the photo list (files, folders with subfolders, drops), the recipe
  (two presets stacked, Auto enhance first, keep edits already made), resize, format and quality,
  folder or a PixelFx folder beside each original, Process N photos; `services::BatchRunner` runs
  two photos at a time on its own pool with Pause, Stop, Retry failed, and each failed row says
  why in plain words. libpixelfx now explains a failed read in plain words instead of libvips's
  developer message. Batch recipes (`.pfxbatch`, presets.md section 4) are read and written by
  the library; the page saves and opens them, and `pixelfx --batch recipe files...` runs one
  without a window (tested end to end on the built binary). The title follows a running batch;
  quitting during one asks first. Rows have thumbnails and a remove button, and the preview row
  shows the first readable photo before and after the recipe. The welcome page has Open folder
  and Batch edit. Not yet: the watermark, the notification at the end.
- 2026-11-05: dev libvips reads HEIC, AVIF, JPEG XL and camera RAW (ADR-008 update):
  `build-deps.sh` builds libheif 1.23.7 (libde265 1.1.3, dav1d 1.5.4), libjxl 0.12.0 (highway
  1.4.0) and LibRaw 0.22.3 against the SDK overlay, about 2.5 minutes from scratch on 14 cores;
  a stamp of all pins rebuilds the prefix when one changes. Checked against the runtime:
  `heifload`, `jxlload`, `jxlsave` and `dcrawload` present; a host-made HEIC, 8 and 10-bit AVIF,
  a JPEG XL written by the dev libvips and a synthetic DNG all decode to the expected values.
  No AVIF or HEIC writing (no encoders). 19 app suites pass.
- Main menu from `menu.html` (F10): Open, Open folder, Open recent, Paste image; Edit, View and
  Photo submenus built from the window's own actions, so each shows the shortcut that works;
  Export, Import presets, Show presets folder, Settings, Keyboard shortcuts, Report a bug, Quit.
  Tab hides the side panels and the filmstrip (S12), only while the photo (a click on it) or
  nothing has the focus, so Tab still walks through sliders and fields. Left out until they
  exist: Save edit with the file, Copy to clipboard (Ctrl+Shift+C is Copy settings, so the mock's
  key clashes), What's new, Online guide, About. 26 app suites pass.
- Watermark (X10) from `watermark.html`: `core::Watermark` (text or logo, Auto, white, black or
  a picked colour, size and margin as shares of the long edge, nine anchors or a dragged
  position, repeat with an angle, soft shadow), drawn with QPainter onto a picture covering
  only the mark, then laid on the exported pixels in linear light by the new
  `pfx::blendOverlay`; Auto reads `pfx::meanLuminance` under the mark. Tokens {year}, {date},
  {camera} from the new `pfx::readExif` (fuzzed: 1.7 million runs clean). Saved watermarks are
  `.pfxwatermark` files (presets.md section 7, new) with the logo copied beside them. The sheet
  opens from a Watermark row in Export and in Batch (section 3, as `batch.html`); Batch no longer
  inherits Export's choice unseen. A watermark file that went missing fails the export instead
  of leaving the mark off. Preview on: This photo, Portrait, Square, and Batch photos when opened
  from Batch (each click decodes the next photo of the batch at preview size on a worker). Not
  yet: keeping the watermark in a `.pfxbatch` (needs batch format version 2, an owner decision). 27 app suites and 90 library
  tests pass.
- The exposure line of `editor.html` under the histogram ("ISO 400 · f/2.8 · 1/250 s · 35 mm",
  `core::exposureLine`), the camera and lens as its tooltip; checked on a scratch JPEG with a
  hand-made Exif block. Photos without exposure data show nothing there.
- LUTs in libpixelfx (L5, ahead of the look step): `pfx::parseCube` (1D and 3D, domain,
  input range, refusing short or long tables with the line number), `pfx::lutFromHald`,
  `pfx::applyLut` (tetrahedral, looked up in sRGB primaries and encoding as display LUTs expect,
  a mix in linear light), `pfx apply --lut FILE[:MIX]`. 0.22 s more for a 33-point cube on a
  24 MP photo (14 threads). A `.cube` fuzzer ran 10.8 million inputs clean. 96 library tests pass.
- Looks in libpixelfx (M4 groundwork, no UI): `pfx::Look` (steps from the registry or `lut`,
  LUT resources) and `pfx::applyLook` with a strength in linear light; `RenderOptions::look`
  runs it after colour grading and before sharpening and the effects (presets.md section 5).
  `.pfxlook` documents read and write (presets.md section 3, now written down exactly as version
  1: params under the operation's group, LUTs as base64 half floats in the `srgb-gamma`
  domain, unknown steps kept and skipped, bad resources left out with a warning). The document
  fuzzer ran 2.2 million inputs with a look seed, clean. 102 library tests pass.
- Copy to clipboard (X8) in the Export sheet footer, as `export.html` has it: the same pixels as
  an export (size, watermark) rendered on a worker (`Exporter::startCopy`, sharing
  `prepareExport` with Export), nothing written to disk; shown for one photo only. 27 app suites
  pass.
- Looks in the app (M4, mock `editor.html`): `core::LookLibrary` with the mock's 13 built-in
  looks (Vivid to Pastel, values are a draft for the owner to tune) built from registry steps,
  the user's looks in `<AppData>/looks` (`.cube` and HaldCLUT import become a one-LUT look),
  `core::findLook` (thread-safe) and `core::renderPhoto`, which every render now goes through
  (editor, thumbnails, Export, Batch). The left panel has Looks and Presets tabs; the Looks tab
  has search, category chips, tiles of the photo under each look (Original first), the applied
  look outlined, and Look strength at its foot (live, one history step on release, keys step by
  one). Choosing a look is one history step on the photo and the selection. Differences from
  the mock, until their pieces exist: no Favorites chip, and "Import LUT..." where the mock has
  "New look" (the look editor, L4). Save as preset has the mock's Look row ("Golden hour 72"),
  so a preset can carry a look. Batch offers the looks in its first choice ("Preset or look", as
  `batch.html`), at each look's own strength; a saved batch recipe refers to a built-in look by
  id, but does not embed a user's look yet (presets.md asks for that, so the file runs on
  another machine). 29 app suites pass.
- The look editor (L4, L6, mock `filter-editor.html`): New look beside My looks, Edit look,
  Export look and Delete on the user's tiles. A step list (registry operations and LUTs, a
  checkbox to switch one off, drag or Move up and down, Duplicate, Remove), the selected step's
  sliders built from the registry (the tone curve with the curve editor), a live preview on the
  photo or a colour ramp rendered on a worker, the default strength, Export look... and Bake to
  .cube... (`pfx::bakeLook`, 33 points, names the steps left out). Saved looks are `.pfxlook`
  files in the looks folder; an edited look re-renders the photo. Differences from the mock:
  Preview on Portrait and Landscape wait for test photos PixelFx can ship; the category list has
  no "Mine" (the user's looks are Mine already). 29 app suites pass.
- A review of the night's changes (a second reader, then checked by hand) found seven bugs, all
  fixed with tests: a look file's LUT with an empty or endless domain, or non-numbers, now is
  left out (it made every pixel NaN); exported looks had no id and never listed; names with a
  leading dot saved hidden files; saving a watermark could replace another of a similar name
  and lose its logo on a failed copy; a preset's Look unit from a photo without a look did not
  take a look off; a look tile was deleted while its own menu was running; a repeated
  watermark of a sliver of a logo could ask for millions of copies. App suites and library
  tests also ran clean under ASan and UBSan (29 and 104).

### 2026-11-04 (M2)

- The app skeleton in `app/`: layered static libraries (core, services, ui, app), the root
  `CMakeLists.txt` building the library and the app together, Qt 6.11 hygiene definitions.
  From the kit: single instance, log sink, changelog reader, icons helper, CMake helpers, test
  helper, dev scripts. Written for PixelFx: settings facade with the one-time PixelFx 1 folder
  migration, command line (files, `--profile`, `--quit`, log file), `Application`, `Document`
  (open and render on a worker, newest request wins), the image bridge, design tokens and style,
  the canvas, the welcome page (from the approved `welcome.html`; Open folder, Batch edit and
  Recent come with their features), the main window header (from `editor.html`), drag and drop,
  debug hooks. libpixelfx gained `toSrgb8` for display.
- ADR-008: `scripts/build-deps.sh` builds libvips 8.18.7 against the KDE snap SDK, so dev builds
  and tests run on the snap's libraries; the runtime farm keeps the host's libstdc++.
- Verified: builds with `-Werror` against the SDK; 10 test suites pass under the snap runtime
  (the library's 34 cases, settings, CLI, single instance, log sink, changelog guard, image
  bridge, document, smoke, and an end-to-end test of the real binary: a second launch hands over
  its photo, `--quit` ends the first). Headless grabs of the welcome page and an opened photo.
- Found and fixed: the fit view used the preview size as the 100% limit; file sizes now in
  decimal units as the mocks show. A manual check of `--quit` was unreliable (it matched the
  wrong process); the end-to-end test replaced it.
- CI (`.github/workflows/ci.yml`, `vcpkg.json`): Linux with warnings as errors, Windows with MSVC
  and vcpkg, macOS with Homebrew. Not run yet; it runs on the next push.

### 2026-11-04 (publishing)

- The rewrite is published in github.com/keshavbhatt/PixelFx (PLAN D20, owner's intent: a clean
  history for Flathub): PixelFx 1 kept on the `legacy-v1` branch and the `v1-final` tag (at
  `568c142`), `main` replaced by the new history (force-push with lease), description updated.
  github.com/keshavbhatt/pixel got an archive note and is archived.
- Before publishing: business notes and keyword research moved to `DOCS/private/` (git-ignored);
  `LICENSING.md`, `REUSE.toml`, `LICENSE` and `LICENSES/` added (`reuse lint` passes); a fresh
  clone builds and passes all tests.

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
