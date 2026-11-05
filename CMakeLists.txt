# PixelFx: the libpixelfx library and the Qt app, built together. The library also builds on
# its own (libs/pixelfx-core).
cmake_minimum_required(VERSION 3.21)

project(pixelfx
    VERSION 2.0.0
    DESCRIPTION "Quick photo editor with looks, presets and batch editing"
    HOMEPAGE_URL "https://github.com/keshavbhatt/PixelFx"
    LANGUAGES CXX
)

option(PIXELFX_BUILD_TESTS "Build the tests (library and app)" ON)
option(PIXELFX_WERROR "Treat warnings as errors" OFF)
if(PIXELFX_BUILD_TESTS)
    enable_testing()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Warnings)
include(Version)

# The test environment for dev builds against the snap runtime (scripts/dev-build.sh); empty
# for plain host builds.
set(PIXELFX_SNAP_RUNTIME "" CACHE PATH "kf6-core24 runtime snap root for running tests (dev builds)")
set(PIXELFX_SNAP_RUNTIME_LIB_PATH "" CACHE STRING "LD_LIBRARY_PATH for the snap runtime (dev builds)")
set(PIXELFX_TEST_ENV "QT_QPA_PLATFORM=offscreen" "QT_LOGGING_RULES=pixelfx.*.debug=false" "QT_FORCE_STDERR_LOGGING=1")
if(PIXELFX_SNAP_RUNTIME)
    list(APPEND PIXELFX_TEST_ENV
        "LD_LIBRARY_PATH=${PIXELFX_SNAP_RUNTIME_LIB_PATH}"
        "QT_PLUGIN_PATH=${PIXELFX_SNAP_RUNTIME}/usr/lib/x86_64-linux-gnu/qt6/plugins")
endif()

# The library first, before the Qt-only compile definitions below.
set(PFX_WERROR ${PIXELFX_WERROR} CACHE BOOL "" FORCE)
set(PFX_BUILD_TESTS ${PIXELFX_BUILD_TESTS} CACHE BOOL "" FORCE)
set(PFX_TEST_ENVIRONMENT "${PIXELFX_TEST_ENV}")
add_subdirectory(libs/pixelfx-core)

# Qt 6.11: the version of the kde-qt6-core24-sdk snap and the Flathub KDE runtime (ADR-000).
set(PIXELFX_QT_MIN_VERSION "6.11" CACHE STRING "Minimum required Qt6 version")
find_package(Qt6 ${PIXELFX_QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Widgets Network Concurrent Svg)
if(UNIX AND NOT APPLE)
    find_package(Qt6 ${PIXELFX_QT_MIN_VERSION} REQUIRED COMPONENTS DBus) # notifications (ADR-011)
endif()
qt_standard_project_setup()
include(SnapSdkWorkaround)

if(MSVC)
    add_compile_options(/utf-8 /bigobj)
endif()
# Qt hygiene: explicit string conversions and keyword-free signal syntax (ADR-000).
add_compile_definitions(
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_KEYWORDS
    QT_NO_FOREACH
    QT_DISABLE_DEPRECATED_UP_TO=0x060B00
    QT_USE_QSTRINGBUILDER
)

add_subdirectory(app)

if(PIXELFX_BUILD_TESTS)
    add_subdirectory(app/tests)
endif()
