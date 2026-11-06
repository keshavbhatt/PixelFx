# The app, layered (CODING_STANDARDS section 2): core -> services -> ui -> app. Each layer is a
# static library so the linker enforces the direction.
# Qt's Linguist tools build the English plural forms (ADR-014); optional.
find_package(Qt6 ${PIXELFX_QT_MIN_VERSION} QUIET COMPONENTS LinguistTools)

add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(ui)
add_subdirectory(app)

qt_add_executable(pixelfx WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(pixelfx PRIVATE pixelfx_app)
pixelfx_set_warnings(pixelfx)
set_target_properties(pixelfx PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/app")

# cmake --build <dir> --target update_translations: refreshes app/i18n/pixelfx_en.ts from the
# sources (plural forms only); fill in the new forms, then commit it.
if(Qt6LinguistTools_FOUND)
    qt_add_lupdate(
        SOURCE_TARGETS pixelfx_core pixelfx_services pixelfx_ui pixelfx_app
        TS_FILES "${PROJECT_SOURCE_DIR}/app/i18n/pixelfx_en.ts"
        LUPDATE_TARGET update_translations
        OPTIONS -pluralonly -no-obsolete -locations none -source-language en -target-language en)
endif()
