# core: app logic with no widgets (settings, log sink, changelog).
qt_add_library(pixelfx_core STATIC
    changelog.cpp
    changelog.h
    log_sink.cpp
    log_sink.h
    logging.cpp
    logging.h
    settings/settings.cpp
    settings/settings.h
    settings/settings_keys.h
)
pixelfx_set_warnings(pixelfx_core)
target_include_directories(pixelfx_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(pixelfx_core PUBLIC Qt6::Core)
