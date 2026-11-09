#!/usr/bin/env bash
# Sourced by dev-build.sh and dev-run.sh. Provides:
#   PIXELFX_RT                       runtime content snap root (kf6-core24)
#   PIXELFX_DEPS                     libvips built against the SDK (scripts/build-deps.sh)
#   pixelfx_prepare_runtime_farm <build-dir>
#   pixelfx_runtime_lib_path     <build-dir>
#   pixelfx_export_runtime_env   <build-dir>
#   pixelfx_runtime_export       <name> <value>
#
# The dev build links against the kde-qt6-core24-sdk snap and runs against the kf6-core24 content
# snap: the libraries the shipped snap uses (adapted from the rewrite kit). Host glibc stays
# authoritative (the content snap carries no libc).

PIXELFX_RT=/snap/kf6-core24/current
PIXELFX_CORE24_BASE=/snap/core24/current/usr/lib/x86_64-linux-gnu
PIXELFX_DEPS="${PIXELFX_DEPS_PREFIX:-${DIR:?}/build/deps/prefix}"

# Some runtime-snap libraries depend on Ubuntu base libraries the host may not have (libselinux,
# liblerc, ...). Farm them from the core24 base snap, excluding the glibc family and the C++
# runtime: the host compiler built this code, so its newer glibc and libstdc++ must stay
# authoritative (the loader chokes on GLIBC_PRIVATE symbols, and code from a newer GCC needs a
# newer libstdc++). The shipped snap is compiled on core24 and has neither problem.
pixelfx_prepare_runtime_farm() {
    local build="$1"
    local farm="$build/core24-libs"
    mkdir -p "$farm"
    local f b
    for f in "$PIXELFX_CORE24_BASE"/*.so*; do
        b="$(basename "$f")"
        case "$b" in
            libc.so*|libm.so*|libmvec.so*|libpthread.so*|libdl.so*|librt.so*|\
            ld-linux*|libresolv.so*|libnsl.so*|libnss_*|libanl.so*|libutil.so*|\
            libBrokenLocale.so*|libthread_db.so*|libstdc++.so*|libgcc_s.so*) continue ;;
        esac
        ln -sf "$f" "$farm/$b"
    done
}

pixelfx_runtime_lib_path() {
    local build="$1"
    local rt="$PIXELFX_RT/usr/lib/x86_64-linux-gnu"
    echo "$PIXELFX_DEPS/lib:$rt:$rt/libproxy:$build/core24-libs"
}

# Exports a variable for the app and keeps the host's value of it, so that the programs the app
# starts (scripts/host-tools) get the host environment back: the runtime's libraries would break
# host programs (its older readline breaks bash, its Qt breaks kde-open).
pixelfx_runtime_export() {
    local name="$1" value="$2"
    case " ${PIXELFX_RUNTIME_VARS:-} " in
        *" $name "*) ;;
        *)
            if [ -n "${!name+x}" ]; then
                export "PIXELFX_HOST_$name=${!name}"
            fi
            export PIXELFX_RUNTIME_VARS="${PIXELFX_RUNTIME_VARS:+$PIXELFX_RUNTIME_VARS }$name"
            ;;
    esac
    export "$name=$value"
}

pixelfx_export_runtime_env() {
    local build="$1"
    local runtime_path
    runtime_path="$(pixelfx_runtime_lib_path "$build")"
    pixelfx_runtime_export LD_LIBRARY_PATH "$runtime_path${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    pixelfx_runtime_export QT_PLUGIN_PATH "$PIXELFX_RT/usr/lib/x86_64-linux-gnu/qt6/plugins"
    # The runtime snap ships only stub Mesa; the shipped snap gets real Mesa from the mesa-2404
    # content snap through the kde-neon-6 extension. Wire the same provider here.
    local mesa=/snap/mesa-2404/current
    if [ -d "$mesa/usr/lib/x86_64-linux-gnu/dri" ]; then
        pixelfx_runtime_export LD_LIBRARY_PATH \
            "$mesa/usr/lib/x86_64-linux-gnu:$mesa/usr/lib/x86_64-linux-gnu/dri:$LD_LIBRARY_PATH"
        pixelfx_runtime_export LIBGL_DRIVERS_PATH "$mesa/usr/lib/x86_64-linux-gnu/dri"
        pixelfx_runtime_export GBM_BACKENDS_PATH "$mesa/usr/lib/x86_64-linux-gnu/gbm"
        pixelfx_runtime_export __EGL_VENDOR_LIBRARY_DIRS "$mesa/usr/share/glvnd/egl_vendor.d"
        pixelfx_runtime_export DRIRC_CONFIGDIR "$mesa/drirc.d"
    fi
    # This Qt build logs to journald when stderr is not a TTY; dev runs want plain stderr.
    pixelfx_runtime_export QT_FORCE_STDERR_LOGGING 1
    # Links and folders the app opens go through xdg-open, found on PATH.
    pixelfx_runtime_export PATH "$DIR/scripts/host-tools:$PATH"
}
