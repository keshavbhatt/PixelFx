# services: work behind the UI (opening and rendering photos through libpixelfx).
qt_add_library(pixelfx_services STATIC
    document.cpp
    document.h
    logging.cpp
    logging.h
    qt_image_bridge.cpp
    qt_image_bridge.h
)
pixelfx_set_warnings(pixelfx_services)
target_link_libraries(pixelfx_services PUBLIC pixelfx_core pfx::core Qt6::Gui Qt6::Concurrent)
