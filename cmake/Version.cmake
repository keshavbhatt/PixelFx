# Derives version metadata for the build.
#
# PIXELFX_VERSION        -> "5.0.0" (from project(VERSION))
# PIXELFX_GIT_REVISION   -> short hash or "unknown"
#
# These are handed to the code via a generated header (see src/app/CMakeLists.txt),
# never via string literals in sources.

set(PIXELFX_VERSION "${PROJECT_VERSION}")

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE PIXELFX_GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT PIXELFX_GIT_REVISION)
    set(PIXELFX_GIT_REVISION "unknown")
endif()

message(STATUS "pixelfx ${PIXELFX_VERSION} (${PIXELFX_GIT_REVISION})")
