# shellcheck shell=bash disable=SC2034 # sourced; the variables are read by the scripts that source it
# The libraries PixelFx is built with, the same in every build (ADR-015): dev builds, Linux CI
# and the snap (scripts/build-deps.sh, snap/snapcraft.yaml), Windows and macOS (vcpkg.json and
# its ports in packaging/vcpkg). scripts/check-pins.py fails when a build file says otherwise.
# Sourced by bash scripts; read line by line by check-pins.py, so keep one NAME=value a line.

# Qt: the version of the kde-qt6-core24-sdk snap and the kf6-core24 runtime the snap runs on.
QT_VERSION=6.11.2

# Built from source everywhere: version, SHA-256 of the release archive, address.
VIPS_VERSION=8.18.7
VIPS_SHA256=5baaead3b0bb20ffdb9e9ff09aa9fda08620923df77b63b436654cb5e0b3bf94
VIPS_URL=https://github.com/libvips/libvips/releases/download/v8.18.7/vips-8.18.7.tar.xz
HWY_VERSION=1.4.0
HWY_SHA256=e72241ac9524bb653ae52ced768b508045d4438726a303f10181a38f764a453c
HWY_URL=https://github.com/google/highway/archive/refs/tags/1.4.0.tar.gz
JXL_VERSION=0.12.0
JXL_SHA256=03e9be69a30be4011f559da75328b6d7cea8ad921fabfbd551ce10bf45cdc992
JXL_URL=https://github.com/libjxl/libjxl/archive/refs/tags/v0.12.0.tar.gz
DAV1D_VERSION=1.5.4
DAV1D_SHA256=686616b7c69eb88d44459391ab25cac13b6647a3b288835c5784e71c1514a5c5
DAV1D_URL=https://downloads.videolan.org/pub/videolan/dav1d/1.5.4/dav1d-1.5.4.tar.xz
DE265_VERSION=1.1.3
DE265_SHA256=554228bd17788c99a7e63b37ab5634722190e6e2bf60c1dcb01cef328e133905
DE265_URL=https://github.com/strukturag/libde265/releases/download/v1.1.3/libde265-1.1.3.tar.gz
HEIF_VERSION=1.23.7
HEIF_SHA256=f7483e818503ea6224c8863e4a56ac02c7089021a98472c811c12aa19faf2475
HEIF_URL=https://github.com/strukturag/libheif/releases/download/v1.23.7/libheif-1.23.7.tar.gz
RAW_VERSION=0.22.3
RAW_SHA256=789f3c33466210a1a0907f747a97c715d21bbf4af712f257fcbbd90111b227e4
RAW_URL=https://www.libraw.org/data/LibRaw-0.22.3.tar.gz
LENSFUN_VERSION=0.3.4
LENSFUN_SHA256=dafb39c08ef24a0e2abd00d05d7341b1bf1f0c38bfcd5a4c69cf5f0ecb6db112
LENSFUN_URL=https://github.com/lensfun/lensfun/archive/refs/tags/v0.3.4.tar.gz

# The image libraries under libvips: on Linux the KDE runtime's (Ubuntu 24.04's), which Qt loads
# into the same process, so no other version can be shipped beside them; Windows and macOS build
# these same versions. build-deps.sh checks the SDK still has them.
JPEG_VERSION=2.1.5
PNG_VERSION=1.6.43
TIFF_VERSION=4.5.1
WEBP_VERSION=1.3.2
LCMS_VERSION=2.16
EXIF_VERSION=0.6.24
ZLIB_VERSION=1.3
BROTLI_VERSION=1.1.0
EXPAT_VERSION=2.6.1
GLIB_VERSION=2.80.0
