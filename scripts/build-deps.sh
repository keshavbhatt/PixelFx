#!/usr/bin/env bash
# Builds libvips from source against the KDE Qt 6.11 snap SDK (kde-qt6-core24-sdk), the same
# libraries the shipped snap links (ADR-003), into build/deps/prefix, together with the codec
# libraries the SDK has no development files for: libheif (HEIC through libde265, AVIF through
# dav1d), libjxl (with highway) and LibRaw. dev-build.sh calls it; it does nothing when the
# pinned versions are already built. The snap builds the same versions as parts, the Flatpak as
# modules.
#
#   scripts/build-deps.sh
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)"
SDK=/snap/kde-qt6-core24-sdk/current
SDK_LIB="$SDK/usr/lib/x86_64-linux-gnu"
RT_LIB=/snap/kf6-core24/current/usr/lib/x86_64-linux-gnu
CORE24_LIB=/snap/core24/current/usr/lib/x86_64-linux-gnu
CMAKE=/snap/cmake/current/bin/cmake
PREFIX="${PIXELFX_DEPS_PREFIX:-$DIR/build/deps/prefix}"
WORK="$DIR/build/deps/work"

# Pinned sources: name, version, SHA-256 of the release archive, URL.
GH=https://github.com
VIPS_VERSION=8.18.7
VIPS_SHA256=5baaead3b0bb20ffdb9e9ff09aa9fda08620923df77b63b436654cb5e0b3bf94
VIPS_URL="$GH/libvips/libvips/releases/download/v$VIPS_VERSION/vips-$VIPS_VERSION.tar.xz"
HWY_VERSION=1.4.0
HWY_SHA256=e72241ac9524bb653ae52ced768b508045d4438726a303f10181a38f764a453c
HWY_URL="$GH/google/highway/archive/refs/tags/$HWY_VERSION.tar.gz"
JXL_VERSION=0.12.0
JXL_SHA256=03e9be69a30be4011f559da75328b6d7cea8ad921fabfbd551ce10bf45cdc992
JXL_URL="$GH/libjxl/libjxl/archive/refs/tags/v$JXL_VERSION.tar.gz"
DAV1D_VERSION=1.5.4
DAV1D_SHA256=686616b7c69eb88d44459391ab25cac13b6647a3b288835c5784e71c1514a5c5
DAV1D_URL="https://downloads.videolan.org/pub/videolan/dav1d/$DAV1D_VERSION/dav1d-$DAV1D_VERSION.tar.xz"
DE265_VERSION=1.1.3
DE265_SHA256=554228bd17788c99a7e63b37ab5634722190e6e2bf60c1dcb01cef328e133905
DE265_URL="$GH/strukturag/libde265/releases/download/v$DE265_VERSION/libde265-$DE265_VERSION.tar.gz"
HEIF_VERSION=1.23.7
HEIF_SHA256=f7483e818503ea6224c8863e4a56ac02c7089021a98472c811c12aa19faf2475
HEIF_URL="$GH/strukturag/libheif/releases/download/v$HEIF_VERSION/libheif-$HEIF_VERSION.tar.gz"
RAW_VERSION=0.22.3
RAW_SHA256=789f3c33466210a1a0907f747a97c715d21bbf4af712f257fcbbd90111b227e4
RAW_URL="https://www.libraw.org/data/LibRaw-$RAW_VERSION.tar.gz"

# Any change to a pin (or to this stamp's format) rebuilds the whole prefix.
STAMP="deps-2 vips-$VIPS_VERSION hwy-$HWY_VERSION jxl-$JXL_VERSION dav1d-$DAV1D_VERSION"
STAMP="$STAMP de265-$DE265_VERSION heif-$HEIF_VERSION raw-$RAW_VERSION"
if [ "$(cat "$PREFIX/.deps-stamp" 2>/dev/null)" = "$STAMP" ]; then
    exit 0
fi
command -v uvx >/dev/null || { echo "uvx (from uv) is needed to run meson: https://docs.astral.sh/uv/" >&2; exit 1; }
command -v ninja >/dev/null || { echo "ninja is needed to build libvips" >&2; exit 1; }
command -v make >/dev/null || { echo "make is needed to build LibRaw" >&2; exit 1; }
[ -x "$CMAKE" ] || { echo "cmake snap missing: sudo snap install cmake --classic" >&2; exit 1; }

started=$SECONDS
rm -rf "$PREFIX"
mkdir -p "$WORK" "$PREFIX"

# fetch <archive name> <sha256> <url>: downloads once, checks, extracts afresh into $WORK/src.
fetch() {
    local tarball="$WORK/$1"
    if [ ! -f "$tarball" ]; then
        curl -fsSL -o "$tarball.part" "$3"
        mv "$tarball.part" "$tarball"
    fi
    echo "$2  $tarball" | sha256sum -c --quiet -
    tar -xf "$tarball" -C "$WORK/src"
}
rm -rf "$WORK/src"
mkdir -p "$WORK/src"
fetch "vips-$VIPS_VERSION.tar.xz" "$VIPS_SHA256" "$VIPS_URL"
fetch "highway-$HWY_VERSION.tar.gz" "$HWY_SHA256" "$HWY_URL"
fetch "libjxl-$JXL_VERSION.tar.gz" "$JXL_SHA256" "$JXL_URL"
fetch "dav1d-$DAV1D_VERSION.tar.xz" "$DAV1D_SHA256" "$DAV1D_URL"
fetch "libde265-$DE265_VERSION.tar.gz" "$DE265_SHA256" "$DE265_URL"
fetch "libheif-$HEIF_VERSION.tar.gz" "$HEIF_SHA256" "$HEIF_URL"
fetch "LibRaw-$RAW_VERSION.tar.gz" "$RAW_SHA256" "$RAW_URL"

# An overlay over the SDK, so the build sees only the snap's libraries:
# - Some of the SDK's development links (libglib-2.0.so, libz.so, ...) point at libraries that
#   live in the runtime snap or the core24 base, not in the SDK; off-snap they dangle and the
#   build would silently take the host's. The overlay links them to the real files, runtime first
#   (what ships).
# - The SDK's lcms2.pc lists two plugin libraries the SDK does not contain; the overlay drops them.
overlay="$WORK/sdk-overlay"
rm -rf "$overlay"
mkdir -p "$overlay/lib" "$overlay/pkgconfig"
for so in "$SDK_LIB"/*.so; do
    [ -e "$so" ] && continue
    target="$(readlink "$so")"
    for base in "$RT_LIB" "$CORE24_LIB"; do
        if [ -e "$base/$target" ]; then
            ln -sf "$base/$target" "$overlay/lib/$(basename "$so")"
            break
        fi
    done
done
for pc in "$SDK_LIB"/pkgconfig/*.pc "$SDK"/usr/share/pkgconfig/*.pc; do
    sed -e "s|-L\${libdir}|-L$overlay/lib -L\${libdir}|" \
        -e 's/ -llcms2_fast_float -llcms2_threaded//' "$pc" > "$overlay/pkgconfig/$(basename "$pc")"
done

# Its bare include directory carries Ubuntu's libc headers, which must not shadow the host
# compiler's: pkg-config drops that -I and the compiler gets the directory with -idirafter
# (searched after the system headers), as SnapSdkWorkaround.cmake does for Qt. What is built
# here comes first, then the overlay.
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$overlay/pkgconfig"
export PKG_CONFIG_SYSTEM_INCLUDE_PATH="$SDK/usr/include"
export CFLAGS="-O2 -idirafter $SDK/usr/include"
export CXXFLAGS="-O2 -idirafter $SDK/usr/include"
link_path="$PREFIX/lib:$overlay/lib:$SDK_LIB:$RT_LIB:$CORE24_LIB"
export LDFLAGS="-L$PREFIX/lib -L$overlay/lib -L$SDK_LIB -Wl,-rpath-link,$link_path"

# CMake projects search only the prefix, the overlay and the SDK (as a cross build would), never
# the host's /usr; when a find_path lands in the SDK's bare include directory, CMake treats it
# as a compiler default and emits no -I for it (the -idirafter above covers it).
cat > "$overlay/sdk-include.cmake" <<EOF
list(APPEND CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES "$SDK/usr/include")
list(APPEND CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "$SDK/usr/include")
EOF
cmake_build() {
    local src="$1" build
    build="$WORK/build-$(basename "$1")"
    shift
    rm -rf "$build"
    "$CMAKE" -S "$src" -B "$build" -G Ninja -Wno-dev \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_PREFIX_PATH="$PREFIX" -DCMAKE_LIBRARY_ARCHITECTURE=x86_64-linux-gnu \
        -DCMAKE_FIND_ROOT_PATH="$PREFIX;$overlay;$SDK" \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
        -DCMAKE_PROJECT_INCLUDE="$overlay/sdk-include.cmake" \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DBUILD_TESTING=OFF "$@"
    ninja -C "$build" install
}

# highway (SIMD for libjxl): static, linked into libjxl, so the runtime's older libhwy.so.1
# is not shadowed.
cmake_build "$WORK/src/highway-$HWY_VERSION" -DBUILD_SHARED_LIBS=OFF \
    -DHWY_ENABLE_CONTRIB=OFF -DHWY_ENABLE_EXAMPLES=OFF -DHWY_ENABLE_TESTS=OFF -DHWY_ENABLE_INSTALL=ON

# libjxl: decoder and encoder, with the SDK's brotli and lcms2 (the release archive carries no
# third_party sources). No tools, plugins or extra codecs.
cmake_build "$WORK/src/libjxl-$JXL_VERSION" -DBUILD_SHARED_LIBS=ON \
    -DJPEGXL_FORCE_SYSTEM_BROTLI=ON -DJPEGXL_FORCE_SYSTEM_LCMS2=ON -DJPEGXL_FORCE_SYSTEM_HWY=ON \
    -DJPEGXL_ENABLE_SKCMS=OFF -DJPEGXL_ENABLE_TOOLS=OFF -DJPEGXL_ENABLE_DEVTOOLS=OFF \
    -DJPEGXL_ENABLE_FUZZERS=OFF -DJPEGXL_ENABLE_DOXYGEN=OFF -DJPEGXL_ENABLE_MANPAGES=OFF \
    -DJPEGXL_ENABLE_BENCHMARK=OFF -DJPEGXL_ENABLE_EXAMPLES=OFF -DJPEGXL_ENABLE_JNI=OFF \
    -DJPEGXL_ENABLE_SJPEG=OFF -DJPEGXL_ENABLE_OPENEXR=OFF -DJPEGXL_ENABLE_TCMALLOC=OFF \
    -DJPEGXL_ENABLE_PLUGINS=OFF -DJPEGXL_ENABLE_VIEWERS=OFF -DJPEGXL_ENABLE_JPEGLI=OFF \
    -DJPEGXL_BUNDLE_LIBPNG=OFF

# dav1d (AV1 decoding for AVIF): static, linked into libheif. Its x86 assembly needs nasm, a
# build tool only; without it the C code paths are used (slower, same output).
dav1d_asm=false
command -v nasm >/dev/null && dav1d_asm=true
rm -rf "$WORK/build-dav1d"
uvx meson setup "$WORK/build-dav1d" "$WORK/src/dav1d-$DAV1D_VERSION" \
    --prefix="$PREFIX" --libdir=lib --buildtype=release --default-library=static \
    -Denable_asm=$dav1d_asm -Denable_tools=false -Denable_examples=false -Denable_tests=false \
    -Denable_docs=false -Dxxhash_muxer=disabled
ninja -C "$WORK/build-dav1d" install

# libde265 (HEVC decoding for HEIC): static, linked into libheif. No tools, no encoder.
cmake_build "$WORK/src/libde265-$DE265_VERSION" -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_SDL=OFF -DENABLE_DECODER=OFF -DENABLE_ENCODER=OFF

# libheif: libde265 and dav1d built in, no plugin loading. No HEVC encoder (x265 is GPL) and no
# AV1 encoder, so HEIC and AVIF are read-only here.
cmake_build "$WORK/src/libheif-$HEIF_VERSION" -DBUILD_SHARED_LIBS=ON \
    -DENABLE_PLUGIN_LOADING=OFF \
    -DWITH_LIBDE265=ON -DWITH_LIBDE265_PLUGIN=OFF -DWITH_DAV1D=ON -DWITH_DAV1D_PLUGIN=OFF \
    -DWITH_X265=OFF -DWITH_X264=OFF -DWITH_OpenH264_DECODER=OFF -DWITH_KVAZAAR=OFF \
    -DWITH_UVG266=OFF -DWITH_VVDEC=OFF -DWITH_VVENC=OFF \
    -DWITH_AOM_DECODER=OFF -DWITH_AOM_ENCODER=OFF -DWITH_SvtEnc=OFF -DWITH_RAV1E=OFF \
    -DWITH_JPEG_DECODER=OFF -DWITH_JPEG_ENCODER=OFF \
    -DWITH_OpenJPEG_DECODER=OFF -DWITH_OpenJPEG_ENCODER=OFF -DWITH_OPENJPH_ENCODER=OFF \
    -DWITH_FFMPEG_DECODER=OFF -DWITH_LIBSHARPYUV=OFF -DWITH_HEADER_COMPRESSION=OFF \
    -DWITH_EXAMPLES=OFF -DWITH_EXAMPLE_HEIF_THUMB=OFF -DWITH_EXAMPLE_HEIF_VIEW=OFF \
    -DWITH_GDK_PIXBUF=OFF -DBUILD_DOCUMENTATION=OFF

# LibRaw (camera RAW), with the SDK's libjpeg (lossy DNG), zlib (deflate DNG) and lcms2. No
# OpenMP (it would pull in the host compiler's libgomp) and no sample programs.
raw_src="$WORK/src/LibRaw-$RAW_VERSION"
(cd "$raw_src" && ./configure --prefix="$PREFIX" --libdir="$PREFIX/lib" \
    --disable-static --disable-openmp --disable-examples \
    --enable-jpeg --enable-zlib --enable-lcms >/dev/null)
make -C "$raw_src" -j"$(nproc)" >/dev/null
make -C "$raw_src" install >/dev/null

# libvips: the formats and colour management PixelFx needs; everything else off so nothing from
# the host leaks in.
rm -rf "$WORK/build"
uvx meson setup "$WORK/build" "$WORK/src/vips-$VIPS_VERSION" \
    --prefix="$PREFIX" --libdir=lib --buildtype=release \
    -Dauto_features=disabled -Ddeprecated=false -Dexamples=false -Dcplusplus=true \
    -Dmodules=disabled -Dintrospection=disabled -Ddocs=false -Dcpp-docs=false -Dvapi=false \
    -Djpeg=enabled -Dpng=enabled -Dtiff=enabled -Dwebp=enabled -Dlcms=enabled -Dexif=enabled \
    -Dzlib=enabled -Dnsgif=true -Dppm=true -Danalyze=false -Dradiance=false \
    -Dheif=enabled -Djpeg-xl=enabled -Draw=enabled
ninja -C "$WORK/build"
ninja -C "$WORK/build" install
echo "$STAMP" > "$PREFIX/.deps-stamp"
echo "libvips $VIPS_VERSION and its codecs built into $PREFIX in $((SECONDS - started)) s"
