# Imaging libraries in the target runtimes (checked 2026-11-04)

Condensed from a runtime check made for `image-libraries.md`: recipes read from
invent.kde.org/packaging/flatpak-kde-runtime, gitlab.com/freedesktop-sdk/freedesktop-sdk and
invent.kde.org/neon/snap-packaging (branch `work.core24`), installed Flatpak runtimes inspected,
and the published `kf6-core24` snap (revision 69) listed file by file. Feeds ADR-003.

## Flathub: org.kde.Platform

| KDE branch | freedesktop base | State |
|---|---|---|
| 6.9 | 24.08 | end of life |
| 6.10 | 25.08 | Qt 6.10.3 |
| **6.11** | **25.08** | **current target**, Qt 6.11.3, kimageformats 6.31.0 |
| 6.12 | 26.08 | beta, not yet on Flathub |

The KDE runtime adds no imaging libraries of its own; they come from freedesktop-sdk.

| Library | fdo 25.08 (KDE 6.11) | fdo 26.08 (KDE 6.12) |
|---|---|---|
| libjpeg-turbo | 3.1.4 | 3.2.1 |
| libpng | 1.6.59 | 1.6.59 |
| libtiff | 4.7.0 | 4.7.2 |
| libwebp | 1.6.0 | 1.6.0 |
| lcms2 | 2.18 | 2.19.1 |
| libexif | 0.6.26 | 0.6.26 |
| libjxl | 0.11.2 | 0.12.0 |
| libavif | 1.3.0 | 1.4.2 |
| libheif | 1.20.2 (AV1, JPEG, JPEG 2000; no HEVC) | 1.23.1 |
| aom, dav1d, svt-av1, highway | 3.13.3, 1.5.1, 3.1.2, 1.3.0 | 3.15.3, 1.5.4, 4.2.0, 1.4.0 |
| libde265, x265 (HEVC) | only in the `org.freedesktop.Platform.codecs-extra` extension | same |
| libvips, LibRaw, Exiv2, OpenEXR, OpenCV, ImageMagick, GEGL | none: build as app modules (flathub/shared-modules has none either) | none |

HEIC decoding: the runtime's libheif loads plugins from the `codecs-extra` directory, which
org.kde.Sdk inherits; when the extension is installed, HEIC opens with no change in the app.
It is not installed by default (patent-restricted codecs).

## Snap: kf6-core24 (content snap, revision 69) and kde-qt6-core24

Ubuntu 24.04 versions, some rebuilt by KDE neon. Apps must not re-stage what the content snap
ships, and must stage what it lacks.

| Library | In kf6-core24 | Version |
|---|---|---|
| libjpeg-turbo, libpng, libtiff, libwebp | yes | 2.1.5, 1.6.43, 4.5.1, 1.3.2 |
| lcms2 | yes | 2.16 (neon) |
| libexif | yes | 0.6.24 |
| LibRaw | yes | 0.21.2 |
| Exiv2 | yes | 0.27.6 (old API) |
| OpenEXR, Imath | yes | 3.1.5, 3.1.9 |
| libheif | yes | 1.17.6, AV1 decoder plugin only, no HEVC |
| libavif, aom, dav1d | yes | 1.0.4, 3.8.2, 1.4.1 |
| highway | yes | 1.3.0 (neon) |
| OpenCV | yes | 4.10 (neon) |
| libjxl | **broken** | ships 0.7 only; its JXL image plugin needs 0.11 |
| libde265 | no | stage `libde265-0` and `libheif-plugin-libde265`, set `LIBHEIF_PLUGIN_PATH` |
| libvips | no | Ubuntu has 8.15.1 (`libvips42t64`); we build 8.18 as a part |

Bug to report to KDE neon: `kimg_jxl.so` in `kf6-core24` links `libjxl.so.0.11` (neon's
rebuilt package was used at build time) while the snap stages Ubuntu's `libjxl0.7`, so JPEG XL
does not load. Apps that need JPEG XL stage or build libjxl themselves.
