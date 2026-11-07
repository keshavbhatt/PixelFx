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
# The crash file names PixelFx's own functions only when the executable exports them (-rdynamic).
if(UNIX AND NOT APPLE)
    set_target_properties(pixelfx PROPERTIES ENABLE_EXPORTS ON)
endif()

# Installing: the program, and on Linux its desktop file, AppStream metainfo and icons, which the
# snap and the Flatpak take from the install tree.
include(GNUInstallDirs)
install(TARGETS pixelfx RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)
if(UNIX AND NOT APPLE)
    set(_pixelfx_linux "${PROJECT_SOURCE_DIR}/app/data/linux")
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/app/data/icons/hicolor" DESTINATION "${CMAKE_INSTALL_DATADIR}/icons")
    install(FILES "${_pixelfx_linux}/com.ktechpit.pixelfx.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${_pixelfx_linux}/com.ktechpit.pixelfx.metainfo.xml" DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
endif()

# cmake --build <dir> --target update_translations: refreshes app/i18n/pixelfx_en.ts from the
# sources (plural forms only); fill in the new forms, then commit it.
if(Qt6LinguistTools_FOUND)
    qt_add_lupdate(
        SOURCE_TARGETS pixelfx_core pixelfx_services pixelfx_ui pixelfx_app
        TS_FILES "${PROJECT_SOURCE_DIR}/app/i18n/pixelfx_en.ts"
        LUPDATE_TARGET update_translations
        OPTIONS -pluralonly -no-obsolete -locations none -source-language en -target-language en)
endif()
