# Analysis: PixelFx v0.0.1 / snap 1.0 (frozen, 2026-11-04)

Written before any rewrite code. Everything the scope contract and the lessons need, from the code itself, not from memory.

Sources:
- `/home/commander/DCode/pixelfx/PixelFx`: Qt5 C++ desktop app. Git: 2020-11-15 (QtCreator stamp in `.pro`) to 2022-02-23 (last commit "fix build"). No tags. About 3,900 lines of own C++/UI plus about 13,000 lines of vendored code.
- `/home/commander/DCode/pixelfx/pixel`: Go command line tool `pixel` (module `ktechpit.com/pixel`, go 1.17, one file `pixel.go`, 327 lines), a thin CLI over `github.com/disintegration/gift` v1.2.1. Last commit 2022-02-22. It is a git submodule of PixelFx at `src/pixel` (empty in this checkout).

Versions disagree: `.pro` says `VERSION = 0.0.1` (shown in About), snap says `version: '1.0'`, the Go tool prints "Pixel v1.0".

---

## 1. Build and stack

**Build system:** qmake, `src/pixelFx.pro`.
- `QT += core gui xml network concurrent widgets`. `xml` and `network` are never used by app code. `concurrent` is used for `QtConcurrent::run`.
- `CONFIG += c++11`. `DEFINES += QT_DEPRECATED_WARNINGS`. Release: `QT_NO_DEBUG_OUTPUT`.
- `DEFINES += VERSIONSTR="0.0.1"` and `APPNAMESTR="$${TARGET}"`. The APPNAMESTR line comes BEFORE `TARGET = pixelfx`, so qmake still has the default TARGET (the `.pro` basename) and the define becomes `"pixelFx"`. That is why window title, dialogs and screenshots say "pixelFx" while the binary is `pixelfx`.
- `include(lib/color_widgets.pri)`, `include(lib/ImageCropper/ImageCropper.pri)`, `include(lib/EasyExif/EasyExif.pri)`: vendored sources compiled into the binary.
- `RESOURCES += icons.qrc theme/qbreeze.qrc`.
- Install: `target -> $PREFIX/bin` (PREFIX default `/usr`), `icon.files = icons/$$TARGET.png` -> `icons/pixelfx.png` into `share/icons/hicolor/512x512/apps/` (BROKEN: file on disk is `icons/pixelFx.png`), `pixelfx.desktop` into `share/applications/`.
- No translations; about 60 `tr()` calls but no translator loaded.

**Qt version:** Qt 5.15.2 from the beineri PPA (`/opt/qt515`) in the snap build. Qt5-only API in places: `QLabel::pixmap()` pointer (`cropwidget.cpp`), `QPainter::drawRoundRect` (ImageCropper).

**Go side:** `go 1.17`, single dependency `github.com/disintegration/gift v1.2.1` (MIT). Built by the snapcraft `go` plugin into `bin/pixel`, moved to `usr/bin/pixel` next to `usr/bin/pixelfx`.

**Vendored third-party code (compiled in):**

| Path | Lines | Origin and license | Purpose / actual use |
|---|---|---|---|
| `src/lib/src/QtColorWidgets/*`, `lib/include/QtColorWidgets/*`, `lib/resources/QtColorWidgets/*` | ~7,000 cpp/ui + ~3,100 hpp | Qt-Color-Widgets, Mattia Basaglia, LGPL-3.0-or-later | Only `HueSlider` (Colorize hue) and `GradientSlider` (Red/Green/Blue/Sepia/Threshold) used; ~20 other widgets compiled but dead. |
| `src/lib/ImageCropper/imagecropperlabel.{h,cpp}` | 882 | Leopard-C ImageCropper (2020-03-06), no license header | Interactive crop label in the Crop dialog. |
| `src/lib/EasyExif/exif.{h,cpp}` | 2,303 | NOT EasyExif: `QExifImageHeader` "copied from Qt Extended 4.5", Nokia 2010, header says "Technology Preview License ... No Commercial Usage ... pre-release code and may not be distributed", LGPL-2.1 alternative | EXIF reading for the Info dialog. |
| `src/widgets/scope/*` | 637 | masc4ii (MLV App, GPL-3.0 upstream), no license text | Only RGB histogram used; vector scope, waveform, parade dead. |
| `src/widgets/waitingSpinner/*` | 393 | Alexander Turkin WaitingSpinnerWidget, MIT | Busy spinner. |
| `src/widgets/elidedLabel/*` | 98 | Eliding label | Compiled, never used. |
| `src/theme/dark.qss`, `light.qss`, `dark/`, `light/` (168 images) | 1,330 + 1,292 qss | Breeze style sheets (MIT lineage) | Only `dark.qss` applied. |
| `src/rungaurd.{h,cpp}` | 111 | Common RunGuard (QSharedMemory + QSystemSemaphore) | Single instance. |
| `src/icons/*-line.png` etc. | 137 files | Remix Icon style (Apache-2.0) | Toolbar and buttons; most unused leftovers. |

---

## 2. Source map

### 2.1 PixelFx (`/home/commander/DCode/pixelfx/PixelFx`)

| Path | Lines | Purpose, notable classes |
|---|---|---|
| `.gitignore` | 52 | Standard Qt ignore list. |
| `.gitmodules` | 3 | Submodule `src/pixel` -> `https://github.com/keshavbhatt/pixel.git`. |
| `LICENSE` | 674 | GPL-3.0 text (README badge claims MIT). |
| `README.md` | 80 | Feature list, snap badge, screenshots. |
| `screenshots/1.jpg`, `2.jpg`, `3.jpg` | binary | UI screenshots (3.12). |
| `snap/snapcraft.yaml` | 132 | Snap recipe (section 5). |
| `snap/gui/icon.png` | binary | 512x512 RGBA app icon. |
| `src/pixelFx.pro` | 109 | qmake project. |
| `src/main.cpp` | 27 | QApplication, name `pixelFx`, org `org.keshavnrj.ubuntu`, version, RunGuard (release only), shows MainWindow. Ignores argv. |
| `src/mainwindow.{h,cpp,ui}` | 65 + 306 + 100 | `MainWindow`: toolbar, central `graphicsView`, right dock "Histogram" holding the filter panel, Fusion + dark palette + `dark.qss`, geometry save/restore, About, Compare press/release via event filter. |
| `src/gift.{h,cpp}` | 63 + 242 | `Gift`: bridge to the Go binary. Owns `ImageLoader`, `giftProcess` (preview), `giftCropProcess` (post-crop), creates a third QProcess for export. Open, apply, compare, crop, save flows. |
| `src/corefilters.{h,cpp,ui}` | 80 + 542 + 1,475 | `coreFilters`: adjustment panel (largest file and `.ui`). Builds the `pixel` flag string, histogram, reset. |
| `src/coreslider.{h,cpp}` | 31 + 81 | `coreSlider` (QSlider without `Q_OBJECT`): click-to-jump, hover value tooltip, `setDefaultValue()`/`reset()`. |
| `src/graphicsview.{h,cpp}` | 65 + 283 | `graphicsView`: one movable `QGraphicsPixmapItem`, fade on update, wheel zoom, Ctrl+Plus/Minus, fit to view, spinner, emits `pixUpdated`. |
| `src/graphicsviewcontrols.{h,cpp,ui}` | 51 + 90 + 188 | Bottom bar: Fit, Rotate left, Rotate right, Flip H, Flip V. Produces `-fliph -flipv -rotate N`. |
| `src/imageloader.{h,cpp}` | 70 + 171 | `ImageLoader`: open dialog, temp dirs, UUID naming, local PNG original and scaled preview PNG. `ImageReader` (QtConcurrent `QImage::load`). |
| `src/cropwidget.{h,cpp,ui}` | 38 + 226 + 332 | `CropWidget`: crop dialog around `ImageCropperLabel` 600x500, shape combo, fixed size, opacity, preview, Cancel/Crop. |
| `src/savedialog.{h,cpp,ui}` | 111 + 181 + 295 | `SaveDialog` ("Image Export"): location, file name, format, PNG/JPEG/TIFF options; `ImageWriter` (QtConcurrent `QImageWriter`). |
| `src/imageinfowidget.{h,cpp,ui}` | 34 + 425 + 78 | `ImageInfoWidget` ("Image Meta"): QImageReader basics + EXIF image/extended/GPS tags. |
| `src/apppreferences.{h,cpp,ui}` | 31 + 77 + 106 | `AppPreferences` ("Settings"): one checkbox, native file dialog. |
| `src/rungaurd.{h,cpp}` | 32 + 79 | `RunGuard`. |
| `src/pixelfx.desktop` | 13 | Desktop entry. |
| `src/icons.qrc` | 139 | 135 icon resources. |
| `src/icons/` | 137 files | Toolbar icons, `app/icon-{16,32,64,128,256,512}.png`, `app/resize.sh`, `pixelFx.png` (512), `categories/` (19, unused), `others/` (unused), `white/` (unused). |
| `src/theme/qbreeze.qrc` | 124 entries | `/qss_icons` images, `/qbreeze/dark.qss`, `/qbreeze/light.qss`. |
| `src/theme/dark.qss`, `light.qss`, `dark/`, `light/` | 1,330 / 1,292 / 168 files | Breeze style sheets and images. |
| `src/lib/color_widgets.pri` | 77 | Adds all QtColorWidgets sources. |
| `src/lib/include/QtColorWidgets/*` | ~3,100 | QtColorWidgets headers. |
| `src/lib/src/QtColorWidgets/*` (21 cpp + 2 ui) | ~7,000 | QtColorWidgets sources; used: `gradient_slider.cpp`, `hue_slider.cpp`. |
| `src/lib/resources/QtColorWidgets/*` | 7 png + qrc | Resources (`alphaback.png` = checkerboard on Sepia track). |
| `src/lib/ImageCropper/*` | 9 + 166 + 716 | Crop widget library. |
| `src/lib/EasyExif/*` | 9 + 353 + 1,950 | QExifImageHeader (misnamed). |
| `src/widgets/scope/Histogram.{h,cpp}` | 28 + 183 | RGB histogram (per-pixel `pixelColor`, log-ish scale). |
| `src/widgets/scope/ScopesLabel.{h,cpp}` | 44 + 104 | QLabel showing scope, 511:160. |
| `src/widgets/scope/VectorScope.*`, `WaveFormMonitor.*` | 137 + 142 | Dead code. |
| `src/widgets/waitingSpinner/*` | 115 + 278 | Spinner. |
| `src/widgets/elidedLabel/*` | 49 + 49 | Dead code. |
| `src/pixel/` | empty | Submodule mount point. |

Biggest own files: `corefilters.ui` (1,475), `corefilters.cpp` (542), `imageinfowidget.cpp` (425), `mainwindow.cpp` (306), `graphicsview.cpp` (283), `gift.cpp` (242). `.ui` files: mainwindow, corefilters, cropwidget, savedialog, imageinfowidget, apppreferences, graphicsviewcontrols (+2 dead in QtColorWidgets).

### 2.2 pixel (`/home/commander/DCode/pixelfx/pixel`)

| Path | Lines | Purpose |
|---|---|---|
| `pixel.go` | 327 | `package main`: flags, decode input file, build gift filter list, encode to stdout. |
| `go.mod` | 5 | `module ktechpit.com/pixel`, `go 1.17`, gift v1.2.1. |
| `go.sum` | 2 | gift checksums. |
| `LICENSE` | 21 | MIT, (c) 2022 Keshav Bhatt. |
| `README.md` | 2 | "pixel / pixelFx client". |
| `.gitignore` | 15 | Go defaults. |

---

## 3. Features as the user sees them

### 3.1 Integration with the Go code

**Not FFI. No c-shared library, no exported C functions, no cgo.** `pixel` is a standalone executable spawned per operation:

```
QProcess::start("bash", {"-c", "./pixel " + filterChain + " \"" + path + "\""})
```

- Working directory = `qApp->applicationDirPath()`, so `pixel` must sit next to `pixelfx`. `bash` required.
- Input: a PNG path (always PNG, 3.3). Output: encoded image bytes on stdout (`png.Encode`, or `jpeg.Encode` quality 100 for JPEG input). App `readAll()`s on finish and `loadFromData`.
- Errors: only `exitCode == 0` checked; stderr never read.

| Flag | Type | Default (off) | Applied when | gift call |
|---|---|---|---|---|
| `-blur` | float | 0 | > 0 | `GaussianBlur(sigma)` |
| `-brightness` | float | -200 | -100..100 | `Brightness(p)` |
| `-hue` | float | -200 | -180..180 | `Hue(shift)` |
| `-contrast` | float | -200 | -100..100 | `Contrast(p)` |
| `-saturation` | float | -200 | -100..500 | `Saturation(p)` |
| `-gamma` | float | 1 | > -1 (always, so `Gamma(1)` always runs) | `Gamma(g)` |
| `-sepia` | float | -1 | 0..100 | `Sepia(p)` |
| `-median` | int | 0 | > 0 | `Median(k, disk=true)` |
| `-mean` | int | 0 | > 0 | `Mean(k, disk=true)` |
| `-min` / `-max` | int | 0 | > 0 | `Minimum/Maximum(k, true)` (unused by app) |
| `-pixelate` | int | 0 | > 0 | `Pixelate(size)` |
| `-threshold` | float | 0 | 1..100 | `Threshold(p)` |
| `-rotate` | int | 0 | 1..360; 90/180/270 exact, else `Rotate(deg, White, Linear)` | counter-clockwise |
| `-gray`, `-invert`, `-transpose`, `-transverse` | bool | | | unused by app |
| `-fliph`, `-flipv` | bool | | | `FlipHorizontal/Vertical()` |
| `-emboss` | bool | | | convolution `-1,-1,0,-1,1,1,0,1,1` (unused) |
| `-edge` | bool | | | convolution `-1 x8, 8` (unused) |
| `-sobel` | bool | | | `Sobel()` (unused) |
| `-resize w,h` | string | | | `Resize(w,h,Lanczos)` (unused) |
| `-resizefit` / `-resizefill` | string | | | SWAPPED: `-resizefit` does `ResizeToFill(Center)`, `-resizefill` does `ResizeToFit` (unused) |
| `-crop x1,y1,x2,y2`, `-cropsize w,h` | string | | | `Crop`, `CropToSize(Center)` (unused) |
| `-unsharp sigma,amount,threshold` | string | | | `UnsharpMask` |
| `-sigmoid midpoint,factor` | string | | | `Sigmoid` |
| `-colorize hue,sat,percent` | string | | | `Colorize` |
| `-colorbalance r,g,b` | string | | | `ColorBalance` (percent) |

- Fixed processing order from the Go source, NOT argument order: blur, brightness, hue, contrast, saturation, gamma, sepia, median, mean, min, max, pixelate, threshold, rotate, gray, invert, transpose, transverse, fliph, flipv, emboss, edge, sobel, resize, resizefit, resizefill, crop, cropsize, unsharp, sigmoid, colorize, colorbalance.
- Decoders: only png and jpeg. Output only for png/jpeg; any other format gives empty output with exit 0. Always 8-bit `NewRGBA`.
- No file argument: prints "Host Application not running, unable to connect. Pixel v1.0", exit 1 (same text for `-h`).

Call sites in `gift.cpp`: (1) `applyFilter` preview on the scaled PNG; (2) `cropAndApplyFilter` after crop; (3) `prepareImageForSaving` on the full-res original PNG, result opens the Save dialog.

### 3.2 Main window layout and toolbar

- Title `pixelFx`, icon `:/icons/app/icon-128.png`. First run maximized, then `mainwindow_geometry`/`mainwindow_windowState` restored.
- Top toolbar (32 px, text under icon, movable only when hovering its first 10 px): **Open** (Ctrl+O), **Save** (Ctrl+S), separator, **Compare**, **Crop**, **Info**, spacer, **Preferences**, **About**. Save/Compare/Crop/Info disabled until an image is shown. Commented out: Undo, Redo, FX, Texture, Frame.
- Center `graphicsView`: dark gradient background, no scrollbars, hand-drag panning, movable item.
- Bottom bar: **Fit** (checkable, default on), **Rotate left** (NOT connected), **Rotate right**, **Flip horizontal**, **Flip vertical** (checkable).
- Right dock "Histogram" (movable/floatable, left or right): histogram, scroll area with collapsible checkable group boxes (100 ms animation), full-width reset button at the bottom.
- About dialog with fade-in: icon, "Designed and Developed by Keshav Bhatt <keshavnrj@gmail.com>", "Website: https://ktechpit.com", "Runtime: Qt Toolkit" (aboutQt), "Version: 0.0.1", "More Apps" -> `https://snapcraft.io/search?q=keshavnrj`, "Source Code" -> `https://github.com/keshavbhatt/SnapHerald` (WRONG repo).
- Shortcuts: Ctrl+O, Ctrl+S, ZoomIn/ZoomOut (1.5x). Wheel zoom 1.05x under cursor. No menu bar, tray, CLI, or drag and drop.

### 3.3 Open / load pipeline and file formats

- `QFileDialog::getOpenFileName` "Open Image", start dir `last_image_dir_path` or Pictures, filter `Image Files (*.png *.jpg *.jpeg *.bmp *.tiff *.webp *.wbmp *mng)` (typo `*mng`; no `.tif`, `.gif`). Native or Qt dialog per `useNativeFileDialog`.
- Actual decoders = Qt image plugins present. `qt515imageformats` is not in the snap, so tiff/webp/wbmp/mng likely fail there.
- Steps: (1) save `last_image_dir_path`, new UUID; (2) `writeOriginalFile()`: copy if `.png`, else decode and re-encode to `<Temp>/pixelFx/original/<uuid>.png` on the GUI thread (to keep transparency for round crops of JPEGs, commit 90c65f4); (3) `QtConcurrent` `QImage::load` of the user file (second decode); (4) `writeScaledFile()` on the GUI thread: if ORIGINAL height > 768 scale to fit 1000*dpr x 768*dpr, write `<Temp>/pixelFx/scaled/<uuid>.png`, re-read it; (5) `Gift::loadImage`: reset filters, show, keep as `scaledImage` ("before").
- Temp dirs `TempLocation/pixelFx/{scaled,original,filtered}` (`filtered` unused), removed on exit; per-session files accumulate.
- No EXIF orientation handling; EXIF/ICC/text metadata lost.

### 3.4 Adjustments and filters (exhaustive)

Each row: label, value label (`xxx_v_lb`), reset button (`xxx_pb`, resets and applies), slider. Wheel swallowed on sliders. Click jumps handle; hover tooltip shows value. **Chain applied only on `sliderReleased`** (and per-row reset); keyboard changes not applied. Panel disabled while `pixel` runs. Chain rebuilt from all controls each time; skipped if equal to the top of `filterStack`.

**Basic Adjustment**

| Label | Object | UI range, default | Sent | gift meaning |
|---|---|---|---|---|
| Brightness | `brightness` | -100..100, 0 | `-brightness v` (omitted at 0) | -100..100 |
| Contrast | `contrast` | -100..100, 0 | `-contrast v` | -100..100 |
| Sigmoidal Contrast | `sigmoidal` | -100..100, 0 | `-sigmoid 0.5,(v/10)` INTEGER division: factor -10..10 in whole steps, -9..9 -> 0 | midpoint 0..1, factor -10..10 |
| Saturation | `saturation` | -100..100, 0 | `-saturation v` | -100..500 |
| Gamma | `gamma` | -100..100, 0 | ALWAYS sent. v >= 0: 1 + v/10 (1.0..11.0); v < 0: 1 + v/100 (0.0..0.99) | out = in^(1/g) |

**Color Balance** (QtColorWidgets GradientSlider, QSlider default range 0..99)

| Label | Object | UI range, default, label | Sent | gift |
|---|---|---|---|---|
| Red | `red` (teal -> dark red) | 0..99, 50; label v-50 | `-colorbalance r,g,b` if any != 50. v=50 -> 0; v>50 -> v-50 (1..49); v<50 -> v-100 (-100..-51): jump and label mismatch | ColorBalance percent, -100..500 |
| Green | `green` (purple -> green) | same | same | same |
| Blue | `blue` (olive -> navy) | same | same | same |
| Sepia | `sepia` (transparent -> 112,66,20) | 0..99, 0 | `-sepia v` | 0..100 |
| Threshold | `threshold` (white -> black) | 0..99, 0 | `-threshold v` | 0..100, black/white |

**Colorize**

| Label | Object | Range, default | Sent |
|---|---|---|---|
| Hue | `HueSlider hueslider` | 0..359, 180 | `-colorize h,i,l` only when Lightness != 0 |
| Intensity | `intensity` | 0..100, 50 | gift saturation 0..100 |
| Lightness | `lightness` | 0..100, 0 | gift percentage 0..100 (mislabelled strength) |

Hue row reset resets all three.

**Hue Shift**: Hue `hslider` -180..180, 0 -> `-hue v`.

**Details**

| Label | Object | Range | Sent |
|---|---|---|---|
| Sharpen | `sharpen` | 0..50, 0 | `-unsharp (v/5),1.0,0.05` (sigma 0.2..10) |
| Pixelate | `pixelate` | 0..50, 0 | `-pixelate v` (cell px) |
| Blur | `gaussianblur` | 0..50, 0 | `-blur v/5` (sigma 0.2..10) |

**Noise Reduction**: Mean filter `mean` 0..10 -> `-mean v`; Median filter `median` 0..10 -> `-median v` (disk kernels; gift expects odd sizes, UI sends even too).

**Geometry**

| Control | Behaviour | Sent |
|---|---|---|
| Fit | Fit on load and resize | nothing |
| Rotate left | Dead button | nothing |
| Rotate right | `(360 - angle) - 90`: toggles 0 <-> 270 only | `-rotate 270` (= 90 cw) |
| Flip H / Flip V | checkable | `-fliph` / `-flipv` |

Geometry changes re-run the whole chain. Rotation resets on new image; flip states do not, and global reset ignores them.

**Histogram**: RGB histogram of displayed (preview) image, recomputed on every update.

**Global reset**: resets sliders, clears `filterStack`, calls `apply()` and `applyFilter("")` (two runs).

**In Go but not in UI:** grayscale, invert, transpose, transverse, emboss, edge, Sobel, min/max, arbitrary rotate, resize/fit/fill, crop rect/size.

### 3.5 Preview, compare, undo, history

- Non-destructive parametric chain re-run from the untouched scaled PNG each time; only crop is destructive.
- Preview on the downscaled copy, export on full resolution with the same pixel-unit parameters: blur, sharpen, pixelate, mean, median look different.
- Swap animation: opacity 1.0 -> 0.7 (100 ms), swap, back (300 ms).
- **Compare**: hold the toolbar button shows the loaded image; release shows filtered. After crop it shows the uncropped image.
- **No undo/redo.** `filterStack` only prevents duplicate applies; Undo/Redo commented out. No history panel, presets, or project file.

### 3.6 Crop

- Loads full-res local original async, opens `CropWidget` ("pixelFx | Crop Dialog", unparented, WindowModal ineffective).
- "Image": `ImageCropperLabel` 600x500, dimmed outside, dashed frame, 8 handles.
- "Preview" 200x200; "Current Selection" Width, Height, X Pos, Y Pos (original px); "Selection Settings": **Cropper** Rect, Square, Fixed Rect, Ellipse, Circle, Fixed Ellipse; **FixedSize** spin boxes (only for fixed shapes, 64x64 default, range 0..image size); **Opacity** 0..100 default 60. Hidden "Shape" output combo; output follows cropper (elliptical shapes -> transparent corners via 1-bit `QBitmap` mask, aliased).
- **Cancel**, **Crop**. Done in Qt (`QPixmap::copy` at `cropperRect / scaledRate`, truncated).
- After Crop: overwrites the local original PNG, regenerates the scaled PNG (can upscale small crops), re-runs the chain, fits view. Not undoable except by reopening.

### 3.7 Save / export

- **Save** (Ctrl+S) runs `pixel <chain>` on the full-res original, then opens `SaveDialog` ("pixelFx | Save Image", "Image Export", ApplicationModal, unparented).
- **File location** + **Select Location**; default `last_image_dir_path` or Pictures.
- **File Name**: `QFileInfo::baseName` of the original (cuts at first dot), placeholder "name without extension", extension label.
- **File format** combo: `QImageWriter::supportedImageFormats()` minus `cur, icns, ico, pbm, pgm, ppm, tif, wbmp, xbm, xpm` (typically bmp, jpeg, jpg, png, tiff, webp). Default original extension (case-sensitive).
- Options: **PNG** "Compression level" 0..100 default 10, passed to `setQuality` (Qt maps inversely, label backwards). **JPEG/JPG** "Quality" 0..100 default 100 + "Comment" (never written: `supportsOption` checked before format/file set). **TIFF** "No compression" (default) / "LZW compression". Others: no options.
- **Save Image**: QtConcurrent write with spinner. No overwrite prompt, no feedback, dialog stays open, result ignored, settings not remembered. JPEG drops alpha of round crops.

### 3.8 Image info

**Info** -> "pixelFx | Image Information Dialog", header "Image Meta": **Basic Info** (File, Format, Size, QImageReader text keys), **Image Tags**, **Other Tags**, **GPS Tags** as `<b>Name</b> : value`, rationals `n/d`. Always the original file; read-only.

### 3.9 Preferences and settings keys

`QSettings` -> `~/.config/org.keshavnrj.ubuntu/pixelFx.conf` (snap: `~/snap/pixelfx/<rev>/.config/`).

| Key | Type, default | Written | Read |
|---|---|---|---|
| `useNativeFileDialog` | bool, true | Preferences "Use Native File Dialog to Open/Save files" | open dialog, save location picker, prefs load |
| `last_image_dir_path` | string, Pictures | on open | open start dir, save default location |
| `themeCombo` | int, 1 (dark) | never | MainWindow: 1 -> dark.qss else light |
| `mainwindow_geometry` | QByteArray | closeEvent | ctor (absent -> maximized) |
| `mainwindow_windowState` | QByteArray | closeEvent | ctor (absent -> centered) |

Theme: `dark.qss` + Fusion + fixed dark palette (Window 53,53,53; Base 42,42,42; Highlight 49,106,150; Link #3DAEE9). Panel gradient `rgba(17,18,20,240)/96/240`. Spinner `#1e90ff`.

### 3.10 Threading

- GUI thread: full-res decode + PNG encode on open, preview scale + encode + re-read, crop encode and overwrite, histogram (3 `pixelColor()` per pixel), `loadFromData` of every `pixel` result.
- QtConcurrent: `QImage::load` on open and before crop; final `QImageWriter::write`.
- Child processes: all filtering in a fresh `bash` + `pixel` per apply, async via QProcess.
- No cancellation; flip/rotate stay enabled while processing and can hit "process already running". `reset()` kills both processes.

### 3.11 Single instance, CLI, desktop

- Release: `RunGuard("org.keshavnrj.ubuntu.pixelFx")`, second launch shows "An instance of pixelFx is already running." and exits; no file forwarding.
- `Exec=pixelfx %F` but argv ignored; no `MimeType=`.

### 3.12 Screenshots

1. `1.jpg`: main window, dark. Toolbar Open, Save | Compare, Crop, Info ... Preferences, About. Street photo in centre, bottom bar with Fit (highlighted), rotate left/right, flip H/V. Right dock: histogram strip, Basic Adjustment (Contrast 0, Sigmoidal Contrast 62, Saturation -69, Gamma 0), Color Balance (Red/Green/Blue gradient tracks, Sepia checkerboard-to-brown, Threshold), reset arrow at bottom. Floating About dialog.
2. `2.jpg`: Image Information Dialog ("Image Meta": portal path, jpeg, 4032 x 3024, Description; samsung SM-G935F, Orientation 1, DateTime; ExposureTime), Save Image dialog ("Image Export": location, file name `20170722_135034` `.jpg`, format jpg, Quality 100, Comment, Save Image), Settings dialog ("General: Use Native File Dialog to Open/Save files").
3. `3.jpg`: Crop Dialog with circle cropper, round preview, Width 2264, Height 2264, X 1283, Y 248, Cropper = Circle, FixedSize disabled, Opacity, Cancel / Crop. Dock shows Colorize (Hue 180 rainbow, Intensity 50, Lightness 0), Hue Shift, Details (Sharpen, Pixelate, Blur), Noise Reduction.

---

## 4. Network and external processes

- **Network: none.** `QT += network`, `<QtNetwork>` include and snap `network` plug unused. Only `QDesktopServices::openUrl` for About links.
- **Child process:** only `bash -c "./pixel <flags> \"<path>\""`, found via working dir = app dir. Stdout = image bytes, exit 0 = success, stderr ignored, no timeout.
- No cookies, sessions, telemetry, update checks.

---

## 5. Packaging and release

**Snap** (`snap/snapcraft.yaml`):
- `name: pixelfx`, `version: '1.0'` (hard-coded), summary "Quick Image retouching application for Linux Desktop", description with typo "proccessing", `grade: stable`, `confinement: strict`, `base: core20` (EOL April 2025), `compression: lzo`, amd64 only, `icon: snap/gui/icon.png`.
- Env `SNAP_DESKTOP_RUNTIME=$SNAP/qt515-core20`; app env `IS_SNAP=1` (unused), `QT_QPA_PLATFORMTHEME=gtk3`.
- beineri `opt-qt-5.15.2-focal` PPA (key C65D51784EDC19A871DBDBB710C56D0DE9977759); `apt install -y build-essential qt515base qt515tools qt515xmlpatterns libgl1-mesa-dev` inside `override-build`.
- Parts: `pixel` (go plugin, `https://github.com/keshavbhatt/pixel.git` unpinned, `build-packages: gcc`, override-prime moves `bin/pixel` to `usr/bin/pixel`); `build-src` (nil, `https://github.com/keshavbhatt/PixelFx.git` master, `source-subdir: src/`, `qmake src`, `make -j4`, `qmake -install qinstall -exe pixelfx`, strip, sed desktop `Icon=` to `${SNAP}/meta/gui/icon.png`); `desktop-launch` (from `https://github.com/keshavbhatt/qt515-core20.git`).
- App: `command: bin/desktop-launch $SNAP/usr/bin/pixelfx`, `desktop: usr/share/applications/pixelfx.desktop`.
- Plugs: `home, unity7, network, opengl, x11, desktop, gsettings, wayland, desktop-legacy, mount-observe`; content `gtk-3-themes`, `icon-themes`, `sound-themes`, `qt515-core20`. No `removable-media`; `network`, `mount-observe` unnecessary.
- No Qt imageformats plugins packaged.
- History: 63e9e9f committed a prebuilt binary + launcher; 1dd95f6 moved to qt515-core20 content snap; a9cb774 moved Go source to a submodule; 91c8792 "symlink pixel client to pixelServer path".

**Flathub, CI, releases:** none. No tags, changelog, CI. Snap built from GitHub master and published (`snap install pixelfx`).

**App id / desktop / icons:** org `org.keshavnrj.ubuntu`, name `pixelFx`, binary and snap `pixelfx`; no reverse-DNS id, no AppStream. Desktop: `Name=PixelFx`, `GenericName=Quick Image Retouching`, `Comment=Quickly retouch your images with various filters`, `Icon=pixelfx`, `Keywords=image;filter;edit;retouch`, `Exec=pixelfx %F`, `Categories=Graphics;Photography;Qt;`, `Terminal=false`, `StartupNotify=false`. Icons `src/icons/app/icon-{16..512}.png` (+ `resize.sh`), `src/icons/pixelFx.png`, `snap/gui/icon.png` (rainbow colour-wheel flower).

---

## 6. Accounts and licensing

**No accounts, trial, paywall, gated features or nags.**

Software licenses:
- PixelFx `LICENSE` GPL-3.0 vs README MIT badge.
- `pixel` MIT; gift MIT.
- QtColorWidgets LGPL-3.0-or-later, statically compiled.
- `lib/EasyExif/exif.*` = Nokia Qt Extended 4.5 code with "may not be distributed" header (LGPL-2.1 alternative). Do not carry over.
- Scope widgets from MLV App (GPL-3.0) without headers; ImageCropper without license text.
- Remix Icon (Apache-2.0) and Breeze qss (MIT) unattributed.

---

## 7. Defects worth not repeating

**Integration / architecture**
1. A new `bash` + Go process for every slider release, with full PNG decode and encode on both sides and the whole image buffered on stdout (`gift.cpp:applyFilter`). High latency, no live drag preview, string-built shell command.
2. Silent failures: exit code only, stderr ignored, unsupported format gives empty output with exit 0 (`pixel.go:321`).
3. Preview (downscaled) and export (full-res) use the same pixel-unit parameters, so they disagree (`imageloader.cpp:writeScaledFile`, `gift.cpp:prepareImageForSaving`).
4. Downscale checks only original height > 768; wide images not scaled, small crops upscaled.
5. `writeScaledFile` dereferences a null `scr`.
6. Go fixed order ignores UI order; gamma always runs.
7. Go flag bugs: `-resizefit`/`-resizefill` swapped, wrong usage texts, no-op `%1` checks, encoder errors ignored, misleading usage message, 16-bit lost.

**Parameter mapping**
8. Color Balance jump to -51 below centre, label mismatch, max +49 (`corefilters.cpp:getRgb` vs `getRgbSliderLabel`).
9. Sigmoid integer division: only 21 effective steps, -9..9 no-op.
10. Gamma asymmetric, reaches degenerate 0.0, always sent.
11. Colorize "Lightness" is strength; Colorize inert at 0.
12. Even kernel sizes for mean/median; saturation capped at 100.
13. Rotate right toggles 0/270 only; Rotate left unconnected.
14. Flip states survive new image and reset.
15. Keyboard slider changes never applied.
16. `coreSlider` without `Q_OBJECT`: `findChildren<coreSlider*>` returns every QSlider and casts GradientSliders to coreSlider; `mouseReleaseEvent` forwards to `mousePressEvent`.

**State, history, crop**
17. No undo/redo.
18. Destructive crop; Compare shows pre-crop image afterwards.
19. 1-bit ellipse mask (jagged); crop coordinates truncated through preview scale.

**Save**
20. Use after free: stack `ImageWriter` captured by `this` in the QtConcurrent lambda (`savedialog.cpp:on_save_button_clicked`, `savedialog.h:ImageWriter::write`).
21. PNG compression slider inverted (`setQuality`), default 10 = near max compression.
22. JPEG comment never written.
23. No overwrite prompt or feedback, dialog stays open, settings not remembered, baseName truncation, alpha lost to JPEG.
24. EXIF/ICC/text dropped; EXIF orientation ignored on load.

**Threading and performance**
25. Full-res decode + PNG encode on GUI thread at open, image decoded twice.
26. Histogram with 3 `pixelColor()` per pixel on GUI thread every update.
27. Flip/rotate enabled during processing; no cancellation.
28. Temp files accumulate in `/tmp/pixelFx/` until exit; survive crashes.

**Build, packaging, UX**
29. `APPNAMESTR` before `TARGET` ("pixelFx" vs "pixelfx"); `.pro` installs non-existent `icons/pixelfx.png`.
30. Version drift 0.0.1 / 1.0 / v1.0; unpinned GitHub master builds; apt in override-build; core20 EOL; amd64 only; imageformats missing.
31. Open filter typo `*mng`, no `.tif`/`.gif`; `%F` ignored; no MimeType, no drag and drop.
32. Wrong "Source Code" link; README MIT vs GPL-3.0; Qt Extended EXIF code.
33. Dead weight: unused QtColorWidgets widgets, scopes, elided label, `filtered/` dir, light theme without UI, `themeCombo` never written, `QT += xml network`, `IS_SNAP`, about 100 unused icons.
34. Crop, save and info dialogs unparented: separate top-level windows, can hide behind main window, WindowModal ineffective.
35. Collapsing a section only hides it; adjustments stay active.

---

## 8. What to keep verbatim

- **Adjustment set and grouping**: Basic Adjustment (Brightness, Contrast, Sigmoidal Contrast, Saturation, Gamma), Color Balance (Red, Green, Blue, Sepia, Threshold), Colorize (Hue, Intensity, Strength), Hue Shift, Details (Sharpen, Pixelate, Blur), Noise Reduction (Mean, Median), Flip H/V, Rotate, Crop shapes Rect/Square/Fixed Rect/Ellipse/Circle/Fixed Ellipse. Keep names and neutral defaults; fix mappings from section 7.
- **gift semantics** (MIT): Brightness/Contrast -100..100, Saturation -100..500, Hue -180..180, Gamma out = in^(1/g), Sigmoid(0.5, -10..10), Sepia/Threshold 0..100, Colorize(h 0..360, s 0..100, p 0..100), ColorBalance(percent), UnsharpMask(sigma, 1.0, 0.05), GaussianBlur(sigma), Pixelate(px), Mean/Median disk kernels. Cheap extras from `pixel.go`: emboss `[-1,-1,0,-1,1,1,0,1,1]`, edge `[-1,-1,-1,-1,8,-1,-1,-1,-1]`, Sobel, Grayscale, Invert, Min/Max, resize/fit/fill.
- **Non-destructive parametric chain** (do it in-process, resolution-aware).
- **Hold-to-compare**.
- **Crop dialog layout**: dimmed outside with handles, live preview, W/H/X/Y readout, shape selector, overlay opacity.
- **Save dialog fields**: location, base name, filtered format list, per-format options (JPEG quality 100, PNG compression, TIFF LZW), with fixes.
- **Info grouping**: Basic Info, Image Tags, Other Tags, GPS Tags (with a properly licensed EXIF reader such as libexif or exiv2).
- **Look**: dark palette values, panel gradient `rgba(17,18,20,240) -> 96 -> 240`, spinner `#1e90ff`, gradient tracks Red (teal -> dark red), Green (purple -> green), Blue (olive -> navy), Sepia (transparent -> 112,66,20), Threshold (white -> black), rainbow Hue track.
- **Strings**: "Quick Image Retouching", "Quickly retouch your images with various filters", "Quick Image retouching application for Linux Desktop", keywords `image;filter;edit;retouch`, categories `Graphics;Photography;Qt;`.
- **Assets**: app icon set `src/icons/app/icon-{16..512}.png` and `snap/gui/icon.png`; the toolbar icons actually used (folder-open, save, compare-image, crop, information, setting, clockwise/anticlockwise, split-cells for flips, focus/fullscreen for fit) with Apache-2.0 attribution added.
- No translations exist.
