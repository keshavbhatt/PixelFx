# Building PixelFx

PixelFx is one repository with two parts (ADR-001):

| Path | What it is |
|---|---|
| `app/` | The PixelFx app: Qt 6.11, C++20, Qt Widgets. GPL-3.0-or-later. |
| `libs/pixelfx-core/` | libpixelfx, the image library, and its `pfx` command line tool: C++20, no Qt. Apache-2.0. |
| `scripts/` | Build, run and packaging helpers. |
| `snap/`, `packaging/` | The snap, the vcpkg ports for Windows and macOS, the store screenshots. |
| `DOCS/` | Plan, features, decisions, design, file formats, standards, progress (`DOCS/README.md`). |

Every build uses the same library versions, pinned in `scripts/deps-pins.sh` (ADR-015):
Qt 6.11.2, libvips 8.18.7, libheif 1.23.7 with libde265 and dav1d, libjxl 0.12.0, LibRaw 0.22.3
and Lensfun 0.3.4. `python3 scripts/check-pins.py` checks that every build file agrees.

## Linux: dev builds

Dev builds are built and run against the KDE snap SDK and runtime the shipped snap uses, so a
dev build behaves as the snap does. You need the `kde-qt6-core24-sdk`, `kf6-core24`, `core24`
and `cmake` snaps, `ninja`, `nasm` (optional, faster AV1 decoding) and `uv` (for meson):

```sh
sudo snap install kde-qt6-core24-sdk kf6-core24 core24
sudo snap install cmake --classic

scripts/dev-build.sh --tests                 # libvips and the codecs, then PixelFx and its tests
scripts/dev-run.sh                           # launch the app
scripts/dev-run.sh -- photo.jpg              # open a photo
scripts/dev-run.sh --ctest                   # run the tests
```

`scripts/dev-build.sh` runs `scripts/build-deps.sh` first, which builds the pinned libraries into
`build/deps/prefix` once and again only when a pin changes. Pass extra CMake options through
`PIXELFX_CMAKE_ARGS`, for example `PIXELFX_CMAKE_ARGS="-DPIXELFX_WERROR=ON"`, as CI does.

### Trying things without touching your own settings

Point `XDG_CONFIG_HOME`, `XDG_DATA_HOME` and `XDG_CACHE_HOME` at a scratch folder, or use
`--profile <name>`. Neither moves the Pictures folder, where Export and Batch save by default:
also write `XDG_PICTURES_DIR="<scratch>/pics"` to `$XDG_CONFIG_HOME/user-dirs.dirs`.

The app has debug hooks for headless checks, listed at the top of `app/src/main.cpp`:

```sh
QT_QPA_PLATFORM=offscreen PIXELFX_DEBUG_OPEN=photo.jpg PIXELFX_DEBUG_GRAB=shot.png scripts/dev-run.sh
```

## Windows and macOS

Windows (MSVC) and macOS (Clang) build every library through vcpkg at the pinned versions, with
the ports in `packaging/vcpkg/ports`, and Qt 6.11.2 (CI fetches it with `scripts/ci/fetch-qt.py`
on Windows and `install-qt-action` on macOS). The exact steps are the `windows` and `macos` jobs
of `.github/workflows/ci.yml`:

```sh
git clone https://github.com/microsoft/vcpkg.git
git -C vcpkg checkout <builtin-baseline from vcpkg.json>
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_TARGET_TRIPLET=x64-windows        # arm64-osx-dynamic on macOS
cmake --build build
ctest --test-dir build --output-on-failure
```

## The library alone

libpixelfx builds and tests on its own with the host's libvips (`libs/pixelfx-core/README.md`):

```sh
make test      # build libpixelfx with -Werror and run its tests (build/lib)
make lint      # clang-format check and clang-tidy
make golden    # regenerate the golden images on purpose, then review them
make fuzz      # libFuzzer targets with ASan and UBSan

build/lib/pfx apply --preset libs/pixelfx-core/examples/moody.pfxpreset -o out/ photo.jpg
```

## CMake options

| Option | Default | What it does |
|---|---|---|
| `PIXELFX_BUILD_TESTS` | `ON` | Build the library's and the app's tests |
| `PIXELFX_WERROR` | `OFF` | Treat warnings as errors (CI turns it on) |
| `PFX_BUILD_TOOLS` | `ON` | Build the `pfx` command line tool |
| `PFX_WITH_LENSFUN` | `AUTO` | Lens profiles through Lensfun: `AUTO`, `ON` or `OFF` |

## Packages

- **Snap:** `snap/snapcraft.yaml`, built by GitHub Actions only (`.github/workflows/snap.yml`)
  after CI passes on `main`, and published to the edge channel.
- **Flathub:** a manifest in Flathub's own repository builds the same pinned libraries as
  modules on the KDE 6.11 runtime.

## Before you send a change

Read `DOCS/CODING_STANDARDS.md` (binding) and `DOCS/README.md` (how the project is tracked).
Run the library tests, `ctest` and the linters. Commits read `area: plain sentence`
(`lib`, `app`, `ui`, `docs`, `snap`, ...). A visible change starts as an HTML mock in
`DOCS/mocks/` (`DOCS/DESIGN.md` section 5).
