# services: work behind the UI (opening and rendering photos through libpixelfx).
qt_add_library(pixelfx_services STATIC
    batch_runner.cpp
    batch_runner.h
    document.cpp
    document.h
    exporter.cpp
    exporter.h
    licensing/license_config.h
    licensing/license_service.cpp
    licensing/license_service.h
    logging.cpp
    logging.h
    notifier.cpp
    notifier.h
    qt_image_bridge.cpp
    qt_image_bridge.h
    render_scheduler.cpp
    render_scheduler.h
    thumbnailer.cpp
    thumbnailer.h
)
pixelfx_set_warnings(pixelfx_services)
target_link_libraries(pixelfx_services PUBLIC pixelfx_core pfx::core account_license Qt6::Gui Qt6::Concurrent)
# Desktop notifications over D-Bus on Linux (ADR-011).
if(TARGET Qt6::DBus)
    target_link_libraries(pixelfx_services PRIVATE Qt6::DBus)
    target_compile_definitions(pixelfx_services PRIVATE PIXELFX_HAS_DBUS)
endif()
