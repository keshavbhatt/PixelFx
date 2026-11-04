# Coding Standards: PixelFx (Qt 6 / C++20 app and the libpixelfx library)

These are the rules the project follows. They are binding for every commit. When a rule is
violated for a good reason, the reason goes in a code comment and (if architectural) in
`DOCS/DECISIONS.md`.

This document is adapted from Omnidict's `DOCS/CODING_STANDARDS.md`, itself from the rewrite
kit (`rewrite-kit/reference/red-docs/CODING_STANDARDS.md`), with the WebEngine and SQLite rules
removed and a section for the Qt-free library added (section 11). The goal is the same:
**small, boring, testable code**.

---

## 1. Language & toolchain

| Item | Rule |
|---|---|
| Language | C++20 (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, no extensions) |
| Qt | Qt 6.11 minimum, everywhere this document or `PLAN.md` mentions a Qt version (see `DECISIONS.md` ADR-000 for the SDK and runtime). No Qt5 compatibility shims. |
| Build | CMake >= 3.21, `qt_standard_project_setup()`, `qt_add_executable`. No qmake. |
| Compiler | GCC >= 12 / Clang >= 15 / MSVC 2022. Warnings on: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor -Wold-style-cast`. Warnings are errors in CI (`-Werror` behind the `PIXELFX_WERROR` (app) and `PFX_WERROR` (library) option). |
| Formatting | `clang-format` with the repo `.clang-format`. Formatting is not reviewed by humans; CI runs `clang-format --dry-run --Werror`. |
| Static analysis | `clang-tidy` with the repo `.clang-tidy` (readability, modernize, bugprone, performance, cppcoreguidelines subset). Runs in CI on changed files. |
| Tests | Qt Test (`QTest`) for the app, doctest for libpixelfx (no Qt), both via CTest. Every non-UI module ships a test. |

---

## 2. Project layout

PixelFx is a monorepo with two CMake projects (ADR-001):

```
pixelfx/
  libs/pixelfx-core/   libpixelfx: standalone CMake project, no Qt, Apache-2.0 (section 11)
    include/pfx/       public headers only
    src/               private implementation, one directory per area: image, color, ops, io, recipe, batch
    tools/pfx/         the pfx command line tool
    tests/             unit and golden-image tests (doctest), tests/golden/, tests/fixtures/
  app/                 Qt 6.11 / C++20 client, GPL-3.0-or-later. Consumes the library by add_subdirectory.
    src/
      app/        entry point, CLI, identity (version.h.in)
      core/       pure app logic, no QtWidgets: settings, sidecars, preset library, history
      services/   document (one open photo), render scheduler, batch runner, importers
      platform/   OS-specific backends behind core interfaces (portal, notifications, colour profile)
      models/     Qt item models for the UI (filmstrip, looks, presets, batch queue)
      ui/         QtWidgets: main_window, canvas, panels, sheets
      modules/AccountAndLicense/   licensing module, its own licence, see LICENSING.md
    tests/        Qt Test, one tst_<module>.cpp per module
  presets/        built-in looks and presets as files (.pfxlook, .pfxpreset), shipped as resources
  scripts/        dev-build.sh, dev-run.sh, snap-runtime-env.sh, screenshots.py
  snap/, packaging/, .github/workflows/
```

Dependency direction inside `app/` is strict and one-way:

```
ui  --> models --> services --> core --> libpixelfx
ui  --> core
platform --> core
app --> everything
```

* libpixelfx never includes a Qt header and never depends on `app/`.
* `app/core/` must compile with only `Qt6::Core` (+ `Qt6::Gui` for `QColor`/`QImage` if unavoidable). No widgets.
* `services/` may use `Qt6::Concurrent` and `core/`.
* `models/` and `ui/` may use everything below them. Nothing depends on `ui/`.
* `platform/` provides interfaces in `core/` and implementations under `platform/<os>/`, selected in CMake, not with `#ifdef` soup inside classes.
* Conversion between `pfx::Image` and `QImage` lives in exactly one file (`services/qt_image_bridge`).

Every app directory is its own `add_library(pixelfx_<name> STATIC)` target with explicit
`target_link_libraries`; the library's areas are object libraries inside one `pfx::core`
target. This makes the dependency arrows enforceable by the linker. Directories are created
in the milestone that needs them, not before: no dead code.

---

## 3. Naming

| Thing | Convention | Example |
|---|---|---|
| Namespace | `pixelfx::<layer>` in the app, `pfx` (and `pfx::detail`) in the library | `pixelfx::ui`, `pfx::Recipe` |
| Class / struct / enum | `PascalCase` | `Recipe`, `RenderScheduler` |
| Method / free function | `camelCase` | `setZoomFactor()` |
| Member variable | `m_camelCase` | `m_recipe` |
| Static member | `s_camelCase` | `s_instance` (should be rare) |
| Constant / constexpr | `kPascalCase` | `constexpr int kRecipeVersion = 1;` |
| Enum values | `PascalCase` in `enum class` | `Theme::Dark` |
| Signals | past tense or noun-ish, no `on` prefix | `previewReady(QImage)` |
| Slots / handlers | verb phrase (no `.ui` auto-connect, so no `on<Source><Event>` convention needed) | `applyTheme()` |
| Files | `snake_case.cpp/.h` matching the primary class: `recipe.h` -> `class Recipe`, `main_window.cpp` -> `class MainWindow` | |
| Settings keys | `section/camelCase` | `"editor/canvasBackground"` |
| Qt properties | `camelCase` | |
| CMake option/env prefix | `PIXELFX_` (app), `PFX_` (library) | `PIXELFX_WERROR`, `PFX_BUILD_TESTS` |
| CMake target prefix | `pixelfx_` (app), `pfx::` alias targets (library) | `pixelfx_core`, `pfx::core` |
| Logging category root | `pixelfx.` (app only; the library logs through a callback) | `pixelfx.render`, `pixelfx.batch` |

One class per file pair. Header guards: `#pragma once`.

---

## 4. Classes & ownership

1. **Qt parent-child ownership** for every `QObject`. `new Foo(this)` is fine; `new Foo()` with a
   later `setParent` is not. A `QObject` without a parent must be held in `std::unique_ptr`
   or `QScopedPointer`, never a bare pointer.
2. Non-`QObject` types: value semantics or `std::unique_ptr`. `std::shared_ptr` requires a
   justification comment.
3. Raw pointers are **non-owning observers only**. Never `delete` a raw pointer outside a
   destructor of the owner.
4. Rule of zero. If you write a destructor, you must explain why.
5. No singletons except a top-level `Application` (via `qApp` cast helper) and a `Settings`
   facade (one instance, owned by `Application`, passed by reference to whoever needs it).
   Nothing else may be reached through a global. **Pass dependencies through constructors.**
6. `Q_OBJECT` on every `QObject` subclass. `Q_DISABLE_COPY_MOVE` on every `QObject` subclass.
7. Prefer composition over inheritance. Do not subclass `QMainWindow` to hold app logic: the
   window owns controllers, it isn't one.
8. Hard size limits: a class > 500 lines or a function > 60 lines is a review blocker. Split it.
9. No `friend`. No `protected` data members.
10. Interfaces (`class INotifier { public: virtual ~INotifier() = default; ... }`) for anything
    with more than one implementation (platform backends, test doubles).

---

## 5. Qt idioms

* **Signals/slots**: always the pointer-to-member-function syntax. `SIGNAL()`/`SLOT()` string
  macros are forbidden. Lambdas must capture `this` only when a context object is given:
  `connect(obj, &Obj::sig, this, [this]{ ... });`, never a context-less lambda on a long-lived
  sender.
* **No `.ui` files.** Build widgets in code. Reasons: diffable, testable, no `ui_*.h` coupling,
  no generated-code mystery. Layouts belong in a private `setupUi()` method that is only layout,
  no logic.
* **Strings**: `QStringLiteral("...")` for literals, `u"..."_s` (`using namespace Qt::StringLiterals`)
  is acceptable and preferred in new code. `tr()` for anything the user sees. No `QString::fromLatin1`
  for constants.
* **Containers**: `QList`/`QHash` when the value crosses a Qt API boundary; `std::` otherwise.
  Iterate with range-for; `qAsConst`/`std::as_const` when iterating a Qt container that would detach.
* **Enums** used in settings or signals are `enum class` registered with `Q_ENUM`/`Q_ENUM_NS`.
* **Timers**: `QTimer::singleShot` with a context object. No busy waiting, no `processEvents()`.
* **Threads**: the GUI thread only paints and handles input. Rendering, decoding, encoding,
  histograms and batch work run on the render scheduler's worker and the library's thread
  pool (ADR-006). No `QThread` subclasses; no `processEvents()`; no `QEventLoop` spins.
  Results come back by queued signal carrying a generation number, and stale generations are
  dropped.
* **Logging**: `Q_LOGGING_CATEGORY` per module (`pixelfx.render`, `pixelfx.batch`, ...).
  `qDebug()` without a category is forbidden. No `std::cout`/`printf`.
* **Deprecated API** is a build error (`QT_DISABLE_DEPRECATED_UP_TO=0x060b00`).
* `QT_NO_CAST_FROM_ASCII`, `QT_NO_CAST_TO_ASCII`, `QT_NO_KEYWORDS` are defined globally
  (use `Q_SIGNALS`/`Q_SLOTS`/`Q_EMIT`).
* Use `QStandardPaths` for every path. Never hardcode `~/.config`, `/tmp`, `AppData`.
* Use `QSettings` **only** through the `core::Settings` facade (typed getters/setters with
  defaults in one place). Direct `QSettings` construction outside `core/settings/` is forbidden.
* Pixels cross between Qt and the library only in `services/qt_image_bridge`.

---

## 6. Error handling

* No exceptions across Qt boundaries. Internal code may use exceptions for programmer
  errors only; never for control flow. Qt is compiled without exception guarantees anyway.
* Fallible operations return `std::optional<T>` or a small `Result<T>` (`pfx/result.h`, reused by the app),
  never a bool plus out-parameter.
* Every failure that the user could care about is logged with a category and surfaced through
  a signal (`errorOccurred(QString userMessage)`), never a `QMessageBox` from inside `core/` or
  `services/`. Only `ui/` shows dialogs.
* `Q_ASSERT` for invariants, never for input validation.

---

## 7. Settings

* One `core::Settings` class, typed accessors, defaults in one table:
  ```cpp
  [[nodiscard]] Theme theme() const;
  void setTheme(Theme);
  Q_SIGNAL void themeChanged(Theme);
  ```
* Keys are `constexpr` in `settings_keys.h`; never spelled twice.
* Settings emit a signal on change; UI reacts to the signal. Nobody polls.
* v1 had five keys under `org.keshavnrj.ubuntu/pixelFx.conf`; only the last image folder is worth migrating (once, on first start).

---

## 8. Testing

* libpixelfx and `app/core/` are pure logic and must have high line coverage (checked in CI, not enforced as a
  hard gate for now).
* `ui/` gets a smoke test: construct `MainWindow` offscreen (`QT_QPA_PLATFORM=offscreen`),
  show, process events, destroy: no crash, no leak warnings.
* Tests use only public API. If something is untestable through public API, the design is wrong.
* Test file naming: `app/tests/tst_<module>.cpp` (library: `libs/pixelfx-core/tests/test_<area>.cpp`, doctest), one `QObject` class per file, `QTEST_MAIN` or
  `QTEST_GUILESS_MAIN`.

---

## 9. Git & review

* Commit style: `area: plain sentence` or `area, area: plain sentence`, for example
  `lib: exposure and contrast operations with golden tests` or
  `ui, models: filmstrip selection`. No conventional-commit prefixes, no trailers.
* One logical change per commit. A commit must build and pass tests on its own.
* Every feature change updates `DOCS/PROGRESS.md` and, if it adds a user-visible option,
  `DOCS/FEATURES.md`.
* No commented-out code. No `TODO` without an issue number: `// TODO(#42): ...`.
* No dead code "for later". Git remembers.

---

## 10. Things explicitly banned

| Banned | Why | Do instead |
|---|---|---|
| `MainWindow` doing decoding, rendering, settings and export | god-class, untestable | separate controllers/services owned by the window |
| Multiple `.cpp` files for one class (`main_window_tray.cpp`, ...) | hides that the class is too big | split the class |
| Direct `QSettings` everywhere with retyped keys | typos, no defaults, no signals | `core::Settings` |
| `#ifdef Q_OS_*` blocks inside business logic | unreadable, untestable | platform backends behind interfaces, chosen in CMake |
| `QMessageBox` / dialogs from non-UI code | couples logic to widgets | signals + UI handles them |
| `SIGNAL()`/`SLOT()` macros, `.ui` files, auto-connect slots | not type-checked, hidden coupling | PMF connects, code-built widgets |
| Global mutable state (`static QString g_...`), `extern` variables | untraceable | constructor injection |
| Third-party code copied into `src/` without a manifest | unmaintained forks | `FetchContent`/submodule with version pinned + `THIRD_PARTY.md` |
| Feature flags via magic settings keys | undiscoverable | explicit option in settings facade + UI |
| Decoding, rendering or encoding on the GUI thread | freezes | the render scheduler and thread pool (ADR-006), signals back to the UI |
| Spawning a process or shell to process an image (v1's `bash -c ./pixel`) | latency, silent failures | libpixelfx in-process (ADR-002) |
| Qt types in libpixelfx, or a GPL dependency in libpixelfx | breaks reuse and the library licence | plain C++ and the dependencies ADR-003 lists |
| Pixel-unit spatial parameters (radius in pixels of the preview) | preview and export disagree | parameters relative to the image size (ADR-004) |

---

## 11. libpixelfx (the library)

* C++20, no Qt, no exceptions across the public API: every fallible call returns
  `pfx::Result<T>` (value or `pfx::Error{code, message}`). Exceptions from dependencies are
  caught at the boundary of the file that calls them.
* Public headers in `include/pfx/` only; nothing from `src/` leaks into them (pimpl where a
  dependency type would). The public API is versioned (`PFX_VERSION_*`) and every break is in
  the library's `CHANGELOG.md`.
* Pixels are `float` linear-light RGBA in the library's working space (ADR-004). 8 and 16 bit
  exist only at the codec boundary.
* Operations are pure functions of (input tile, parameters, image geometry); no global state,
  no hidden caches outside the pipeline's cache object. Every operation is registered once with
  its parameter schema (id, type, range, default, unit, version); nothing else in the library
  or the app spells its parameter names.
* Spatial parameters are fractions of the image's long edge, never pixels.
* Every operation has a golden test (fixture in, expected image out, tolerance stated) and a
  scale test (half-resolution render matches the downscaled full render within tolerance).
* Format parsers (recipe, preset, look, LUT, XMP) are fuzzed (`tests/fuzz/`, libFuzzer) and
  never trust sizes from a file.
* Logging through a callback the host sets (`pfx::setLogSink`); no `printf`, no `std::cout`
  outside `tools/pfx`.
* Dependencies: only those named in ADR-003, each with a `THIRD_PARTY.md` row, permissive or
  LGPL (dynamically linked); found with `find_package`, fetched with `FetchContent` at a pinned
  tag when the platform lacks them.
* The library builds and passes its tests on its own (`cmake -S libs/pixelfx-core`) on Linux,
  Windows (MSVC) and macOS (Clang) in CI.

---

## 12. Golden images and fixtures

* `libs/pixelfx-core/tests/fixtures/` holds small, owned or public-domain test images (a
  gradient ramp, a colour chart, a portrait and a landscape we have the rights to, a 16-bit
  TIFF, one RAW per supported make where licences allow); each has a provenance line in
  `fixtures/README.md`.
* Golden outputs live under `tests/golden/` as 16-bit PNG and are compared with a stated
  tolerance (max and mean delta E). They are regenerated only deliberately with
  `make golden` (`PFX_GOLDEN_UPDATE=1`), and the diff is reviewed like any other change, never regenerated to
  make a failing test pass without reading why it failed.
* Preset, look and recipe files in `presets/` are loaded by a test that renders each on the
  fixtures, so a built-in look can never ship broken.

---

## 13. Shell

* `#!/usr/bin/env bash` and `set -euo pipefail` at the top of every script.
* `shellcheck` clean.

---

## 14. Definition of Done (per feature)

- [ ] Code follows every section above; `clang-format` and `clang-tidy` clean; library tests and golden images pass for library changes.
- [ ] Unit tests for all library and `app/core/` logic touched; smoke test still passes.
- [ ] Works on Linux (X11 + Wayland); Windows/macOS-specific code behind platform backends.
- [ ] Logging category added/used; no silent failures.
- [ ] `DOCS/FEATURES.md` row updated (status, implementing class).
- [ ] `DOCS/PROGRESS.md` entry.
- [ ] No new third-party dependency without an ADR in `DOCS/DECISIONS.md` and a row in `THIRD_PARTY.md`.
