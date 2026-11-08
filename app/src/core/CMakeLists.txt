# core: app logic with no widgets (settings, history, log sink, changelog).
qt_add_library(pixelfx_core STATIC
    about.cpp
    about.h
    batch_resources.cpp
    batch_resources.h
    bug_report.cpp
    bug_report.h
    changelog.cpp
    changelog.h
    compress_job.cpp
    compress_job.h
    crash_handler.cpp
    crash_handler.h
    crop_geometry.cpp
    crop_geometry.h
    describe.cpp
    describe.h
    export_options.cpp
    export_options.h
    file_names.cpp
    file_names.h
    histogram.cpp
    histogram.h
    licensing/pro_features.cpp
    licensing/pro_features.h
    history.cpp
    history.h
    links.h
    log_sink.cpp
    log_sink.h
    logging.cpp
    looks.cpp
    looks.h
    logging.h
    photo_details.cpp
    plurals.cpp
    plurals.h
    photo_details.h
    presets.cpp
    presets.h
    reveal_dust.cpp
    reveal_dust.h
    sidecar_store.cpp
    sidecar_store.h
    watermark.cpp
    watermark.h
    settings/settings.cpp
    settings_clipboard.cpp
    settings_clipboard.h
    settings/settings.h
    settings/settings_keys.h
)
pixelfx_set_warnings(pixelfx_core)
target_include_directories(pixelfx_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(pixelfx_core PUBLIC Qt6::Core Qt6::Gui Qt6::Svg pfx::core)

# Built-in texts: the changelog for What's new and the licence texts for About.
set(_pixelfx_resources "${CMAKE_CURRENT_SOURCE_DIR}/../resources")
file(GLOB _pixelfx_notices "${_pixelfx_resources}/notices/*.txt")
qt_add_resources(pixelfx_core "pixelfx_notices"
    PREFIX "/"
    BASE "${_pixelfx_resources}"
    FILES ${_pixelfx_notices})
# The built-in preset catalog (FEATURES P9): one .pfxpreset per preset, a folder per category.
file(GLOB_RECURSE _pixelfx_catalog "${_pixelfx_resources}/presets/*.pfxpreset")
if(_pixelfx_catalog)
    qt_add_resources(pixelfx_core "pixelfx_presets"
        PREFIX "/"
        BASE "${_pixelfx_resources}"
        FILES ${_pixelfx_catalog})
endif()
qt_add_resources(pixelfx_core "pixelfx_changelog"
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES "${PROJECT_SOURCE_DIR}/CHANGELOG.md")

# English plural forms ("1 photo", "2 photos"): tr("%n photos", nullptr, n) needs a translation
# even in the source language. Built when Qt's Linguist tools are there (ADR-014); without them
# every count reads in the plural. Found in app/src/CMakeLists.txt.
if(Qt6LinguistTools_FOUND)
    qt_add_lrelease(
        TS_FILES "${PROJECT_SOURCE_DIR}/app/i18n/pixelfx_en.ts"
        QM_FILES_OUTPUT_VARIABLE _pixelfx_qm)
    qt_add_resources(pixelfx_core "pixelfx_i18n"
        PREFIX "/i18n"
        BASE "${CMAKE_CURRENT_BINARY_DIR}"
        FILES ${_pixelfx_qm})
else()
    message(WARNING "Qt Linguist tools not found: counts will read in the plural (\"1 photos\")")
endif()
