#!/usr/bin/env bash
# Builds libvips from source against the KDE Qt 6.11 snap SDK (kde-qt6-core24-sdk), the same
# libraries the shipped snap links (ADR-003), into build/deps/prefix. dev-build.sh calls it; it
# does nothing when the pinned version is already built. The snap builds the same version as a
# part, the Flatpak as a module.
#
#   scripts/build-deps.sh
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)"
SDK=/snap/kde-qt6-core24-sdk/current
SDK_LIB="$SDK/usr/lib/x86_64-linux-gnu"
RT_LIB=/snap/kf6-core24/current/usr/lib/x86_64-linux-gnu
CORE24_LIB=/snap/core24/current/usr/lib/x86_64-linux-gnu
VIPS_VERSION=8.18.7
VIPS_SHA256=5baaead3b0bb20ffdb9e9ff09aa9fda08620923df77b63b436654cb5e0b3bf94
PREFIX="${PIXELFX_DEPS_PREFIX:-$DIR/build/deps/prefix}"
WORK="$DIR/build/deps/work"

if [ "$(cat "$PREFIX/.vips-version" 2>/dev/null)" = "$VIPS_VERSION" ]; then
    exit 0
fi
command -v uvx >/dev/null || { echo "uvx (from uv) is needed to run meson: https://docs.astral.sh/uv/" >&2; exit 1; }
command -v ninja >/dev/null || { echo "ninja is needed to build libvips" >&2; exit 1; }

mkdir -p "$WORK"
tarball="$WORK/vips-$VIPS_VERSION.tar.xz"
if [ ! -f "$tarball" ]; then
    curl -fsSL -o "$tarball.part" \
        "https://github.com/libvips/libvips/releases/download/v$VIPS_VERSION/vips-$VIPS_VERSION.tar.xz"
    mv "$tarball.part" "$tarball"
fi
echo "$VIPS_SHA256  $tarball" | sha256sum -c --quiet -
src="$WORK/vips-$VIPS_VERSION"
[ -d "$src" ] || tar -xf "$tarball" -C "$WORK"

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
# (searched after the system headers), as SnapSdkWorkaround.cmake does for Qt.
export PKG_CONFIG_LIBDIR="$overlay/pkgconfig"
export PKG_CONFIG_SYSTEM_INCLUDE_PATH="$SDK/usr/include"
export CFLAGS="-idirafter $SDK/usr/include"
export CXXFLAGS="-idirafter $SDK/usr/include"
link_path="$overlay/lib:$SDK_LIB:$RT_LIB:$CORE24_LIB"
export LDFLAGS="-L$overlay/lib -L$SDK_LIB -Wl,-rpath-link,$link_path"

# The formats and colour management PixelFx needs; everything else off so nothing from the
# host leaks in. HEIF, JPEG XL and RAW arrive with M3 (the SDK lacks their development files).
rm -rf "$WORK/build"
uvx meson setup "$WORK/build" "$src" \
    --prefix="$PREFIX" --libdir=lib --buildtype=release \
    -Dauto_features=disabled -Ddeprecated=false -Dexamples=false -Dcplusplus=true \
    -Dmodules=disabled -Dintrospection=disabled -Ddocs=false -Dcpp-docs=false -Dvapi=false \
    -Djpeg=enabled -Dpng=enabled -Dtiff=enabled -Dwebp=enabled -Dlcms=enabled -Dexif=enabled \
    -Dzlib=enabled -Dnsgif=true -Dppm=true -Danalyze=false -Dradiance=false
ninja -C "$WORK/build"
ninja -C "$WORK/build" install
echo "$VIPS_VERSION" > "$PREFIX/.vips-version"
echo "libvips $VIPS_VERSION built into $PREFIX"
