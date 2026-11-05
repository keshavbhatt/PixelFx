#!/usr/bin/env bash
# Builds PixelFx on the host against the KDE Qt 6.11 snap SDK (kde-qt6-core24-sdk), the Qt the
# shipped snap uses at runtime from the kf6-core24 content snap, with the cmake snap the snap
# build uses, and libvips built against the same SDK (scripts/build-deps.sh). What compiles here
# is what ships (adapted from the rewrite kit).
#
#   scripts/dev-build.sh            # RelWithDebInfo into build/app
#   PIXELFX_BUILD_TYPE=Debug scripts/dev-build.sh
#   PIXELFX_CMAKE_ARGS="-DPIXELFX_WERROR=ON" scripts/dev-build.sh
#   scripts/dev-build.sh --tests    # also run ctest afterwards
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)"
SDK=/snap/kde-qt6-core24-sdk/current
SDK_LIB="$SDK/usr/lib/x86_64-linux-gnu"
BUILD="${PIXELFX_BUILD_DIR:-$DIR/build/app}"
CMAKE=/snap/cmake/current/bin/cmake

# shellcheck source=snap-runtime-env.sh
source "$DIR/scripts/snap-runtime-env.sh"

if [ ! -d "$SDK_LIB/cmake/Qt6" ]; then
    echo "Qt SDK snap missing: sudo snap install kde-qt6-core24-sdk" >&2; exit 1
fi
if [ ! -x "$CMAKE" ]; then
    echo "cmake snap missing (the cmake the snap build uses): sudo snap install cmake --classic" >&2; exit 1
fi
if [ ! -d "$PIXELFX_RT/usr/lib/x86_64-linux-gnu" ]; then
    echo "kf6-core24 runtime snap missing (needed to run and test): sudo snap install kf6-core24" >&2; exit 1
fi

"$DIR/scripts/build-deps.sh"
pixelfx_prepare_runtime_farm "$BUILD"

# The SDK's own build tools (moc, rcc, ...) need the SDK's libraries; a few libraries those need
# are not on every host: link just the missing ones from core24 or the runtime snap.
sdk_path="$SDK_LIB:$SDK_LIB/libproxy"
build_libs="$BUILD/build-libs"
mkdir -p "$build_libs"
missing="$(for tool in "$SDK"/usr/bin/qt6/* "$SDK"/usr/lib/qt6/libexec/*; do
    [ -x "$tool" ] && LD_LIBRARY_PATH="$sdk_path" ldd "$tool" 2>/dev/null
done | awk '/=> not found/ {print $1}' | sort -u)"
for lib in $missing; do
    for from in "$PIXELFX_CORE24_BASE" "$PIXELFX_RT/usr/lib/x86_64-linux-gnu"; do
        if [ -e "$from/$lib" ]; then
            ln -sf "$from/$lib" "$build_libs/$lib"
            break
        fi
    done
done
export LD_LIBRARY_PATH="$sdk_path:$build_libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# libvips and its dependencies come from the SDK overlay that build-deps.sh prepared; the SDK's
# bare include directory goes after the host's libc headers (see build-deps.sh).
overlay="$DIR/build/deps/work/sdk-overlay"
export PKG_CONFIG_LIBDIR="$PIXELFX_DEPS/lib/pkgconfig:$overlay/pkgconfig"
export PKG_CONFIG_SYSTEM_INCLUDE_PATH="$SDK/usr/include"
link_path="$PIXELFX_DEPS/lib:$overlay/lib:$SDK_LIB:$PIXELFX_RT/usr/lib/x86_64-linux-gnu:$PIXELFX_CORE24_BASE"

# The SDK keeps its CMake packages under lib/x86_64-linux-gnu, which CMake searches only on a
# Debian-style host; elsewhere Qt's tool packages (moc, rcc, uic) would come from the host's Qt.
# "$SDK_LIB/cmake" finds them in the SDK, and -U drops any found on the host before.
read -r -a EXTRA_ARGS <<< "${PIXELFX_CMAKE_ARGS:-}"
"$CMAKE" -S "$DIR" -B "$BUILD" -G Ninja \
    -U 'Qt6*Tools_DIR' \
    -DCMAKE_BUILD_TYPE="${PIXELFX_BUILD_TYPE:-RelWithDebInfo}" \
    -DCMAKE_PREFIX_PATH="$SDK_LIB/cmake;$SDK/usr" \
    -DQt6_DIR="$SDK_LIB/cmake/Qt6" \
    -DCMAKE_CXX_FLAGS="-idirafter $SDK/usr/include" \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,-rpath-link,$link_path -Wl,--allow-shlib-undefined" \
    -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-rpath-link,$link_path" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DPIXELFX_SNAP_RUNTIME="$PIXELFX_RT" \
    -DPIXELFX_SNAP_RUNTIME_LIB_PATH="$(pixelfx_runtime_lib_path "$BUILD")" \
    "${EXTRA_ARGS[@]}"
"$CMAKE" --build "$BUILD" -j"$(nproc)"

# clangd and IDEs look for compile_commands.json at the repository root.
ln -sf "$BUILD/compile_commands.json" "$DIR/compile_commands.json"

if [ "${1:-}" = "--tests" ]; then
    "$DIR/scripts/dev-run.sh" --ctest
fi
