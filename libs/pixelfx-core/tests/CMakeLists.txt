FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
    URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
    FIND_PACKAGE_ARGS 2.4)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(doctest)

add_executable(pfx_tests
    test_main.cpp
    fixtures.cpp
    test_auto.cpp
    test_color.cpp
    test_document.cpp
    test_geometry.cpp
    test_effects.cpp
    test_mixer.cpp
    test_grading.cpp
    test_registry.cpp
    test_ops.cpp
    test_pipeline.cpp
    test_io.cpp
    test_resample.cpp
)
target_include_directories(pfx_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pfx_tests PRIVATE pfx::core doctest::doctest PkgConfig::VIPS)
target_compile_definitions(pfx_tests PRIVATE PFX_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
pfx_set_warnings(pfx_tests)

# Inside a parent build that needs a special runtime environment (the PixelFx app's snap-runtime
# dev builds), the parent sets PFX_TEST_ENVIRONMENT and the suite runs as one test with it;
# standalone, every test case is its own CTest test.
if(DEFINED PFX_TEST_ENVIRONMENT)
    add_test(NAME pfx_tests COMMAND pfx_tests)
    set_tests_properties(pfx_tests PROPERTIES ENVIRONMENT "${PFX_TEST_ENVIRONMENT}" LABELS "unit")
else()
    include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
    doctest_discover_tests(pfx_tests)
endif()
