# libFuzzer targets (clang only): cmake -DPFX_BUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER=clang++.
# Run with `make fuzz` from the repository root; findings go to build/fuzz/crashes.
foreach(target fuzz_document fuzz_load fuzz_exif)
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE pfx::core)
    target_compile_options(${target} PRIVATE -fsanitize=fuzzer)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer)
endforeach()
