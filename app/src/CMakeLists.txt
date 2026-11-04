# The app, layered (CODING_STANDARDS section 2): core -> services -> ui -> app. Each layer is a
# static library so the linker enforces the direction.
add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(ui)
add_subdirectory(app)

qt_add_executable(pixelfx WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(pixelfx PRIVATE pixelfx_app)
pixelfx_set_warnings(pixelfx)
set_target_properties(pixelfx PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/app")
