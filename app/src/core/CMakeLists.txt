# core: app logic with no widgets (settings, history, log sink, changelog).
qt_add_library(pixelfx_core STATIC
    changelog.cpp
    changelog.h
    crop_geometry.cpp
    crop_geometry.h
    export_options.cpp
    export_options.h
    histogram.cpp
    histogram.h
    history.cpp
    history.h
    log_sink.cpp
    log_sink.h
    logging.cpp
    logging.h
    photo_details.cpp
    photo_details.h
    presets.cpp
    presets.h
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
