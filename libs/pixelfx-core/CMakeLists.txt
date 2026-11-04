# libpixelfx: the PixelFx image library. Standalone: builds and tests without the app.
cmake_minimum_required(VERSION 3.21)
project(pixelfx_core VERSION 0.1.0 DESCRIPTION "Non-destructive photo processing library" LANGUAGES CXX)

option(PFX_BUILD_TESTS "Build the library tests" ${PROJECT_IS_TOP_LEVEL})
option(PFX_BUILD_TOOLS "Build the pfx command line tool" ON)
option(PFX_WERROR "Treat warnings as errors" OFF)
option(PFX_BUILD_FUZZERS "Build the libFuzzer targets (clang, with sanitizers)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PfxWarnings)
include(FetchContent)

find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips-cpp>=8.15)
pkg_check_modules(LCMS2 REQUIRED IMPORTED_TARGET lcms2>=2.14)
find_package(Threads REQUIRED)

# JSON for the document formats (ADR-007). Private: never in the public headers.
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
    FIND_PACKAGE_ARGS 3.12)
FetchContent_MakeAvailable(nlohmann_json)

if(PFX_BUILD_FUZZERS)
    # Coverage for the fuzzer and the address and undefined-behaviour sanitizers on the whole
    # library, so the fuzzers find their way in and see what goes wrong.
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(pfx_core
    src/core/log.cpp
    src/core/parallel.cpp
    src/color/color.cpp
    src/color/lut.cpp
    src/image/image.cpp
    src/image/filters.cpp
    src/image/resample.cpp
    src/image/composite.cpp
    src/recipe/settings.cpp
    src/recipe/document.cpp
    src/recipe/encoding.cpp
    src/recipe/xmp_import.cpp
    src/ops/registry.cpp
    src/ops/auto.cpp
    src/ops/curve.cpp
    src/ops/curve_public.cpp
    src/ops/light.cpp
    src/ops/color_ops.cpp
    src/ops/curves_op.cpp
    src/ops/detail.cpp
    src/ops/geometry.cpp
    src/ops/effects.cpp
    src/ops/mixer.cpp
    src/ops/grading.cpp
    src/ops/dehaze.cpp
    src/pipeline/pipeline.cpp
    src/pipeline/look.cpp
    src/io/io.cpp
    src/io/metadata.cpp
)
add_library(pfx::core ALIAS pfx_core)
target_include_directories(pfx_core
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(pfx_core
    PRIVATE PkgConfig::VIPS PkgConfig::LCMS2 nlohmann_json::nlohmann_json Threads::Threads)
target_compile_definitions(pfx_core PRIVATE
    PFX_VERSION_STRING="${PROJECT_VERSION}")
pfx_set_warnings(pfx_core)

if(PFX_BUILD_TOOLS)
    add_executable(pfx tools/pfx/main.cpp)
    target_link_libraries(pfx PRIVATE pfx::core)
    pfx_set_warnings(pfx)
endif()

if(PFX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PFX_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()
