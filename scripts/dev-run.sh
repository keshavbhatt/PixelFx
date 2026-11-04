#!/usr/bin/env bash
# Runs the dev build against the kf6-core24 runtime content snap: the libraries the shipped
# snap uses. Host fonts, display and D-Bus work directly.
#
#   scripts/dev-run.sh                 # launch the app
#   scripts/dev-run.sh --ctest [args]  # run the test suite (environment baked in by CMake)
#   scripts/dev-run.sh -- photo.jpg    # pass arguments to pixelfx
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${PIXELFX_BUILD_DIR:-$DIR/build/app}"

# shellcheck source=snap-runtime-env.sh
source "$DIR/scripts/snap-runtime-env.sh"

[ -d "$PIXELFX_RT/usr/lib/x86_64-linux-gnu" ] || {
    echo "kf6-core24 runtime snap missing: sudo snap install kf6-core24" >&2; exit 1; }

if [ "${1:-}" = "--ctest" ]; then
    # ctest from the cmake snap (libc only). The tests get the runtime environment through their
    # CTest ENVIRONMENT property (app/tests/CMakeLists.txt).
    cd "$BUILD"
    exec /snap/cmake/current/bin/ctest --output-on-failure "${@:2}"
fi

pixelfx_prepare_runtime_farm "$BUILD"
pixelfx_export_runtime_env "$BUILD"
# File dialogs and file opening go through xdg-desktop-portal, as in the shipped packages.
export QT_QPA_PLATFORMTHEME="${QT_QPA_PLATFORMTHEME_OVERRIDE:-xdgdesktopportal}"
export QT_LOGGING_RULES="${QT_LOGGING_RULES:-pixelfx.*.debug=true}"

[ "${1:-}" = "--" ] && shift
cd "$DIR"
exec "$BUILD/app/pixelfx" "$@"
