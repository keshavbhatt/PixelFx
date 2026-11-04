# ui: widgets. Tokens come from DOCS/DESIGN.md section 1; screens follow the approved mocks.
qt_add_library(pixelfx_ui STATIC
    adjust_panel.cpp
    adjust_panel.h
    canvas.cpp
    canvas.h
    crop_overlay.h
    crop_panel.cpp
    crop_panel.h
    curve_editor.cpp
    curve_editor.h
    export_sheet.cpp
    export_sheet.h
    histogram_view.cpp
    histogram_view.h
    icons.cpp
    icons.h
    main_window.cpp
    main_window.h
    settings_sheet.cpp
    settings_sheet.h
    style.cpp
    style.h
    straighten_dial.cpp
    straighten_dial.h
    welcome_page.cpp
    welcome_page.h
)
pixelfx_set_warnings(pixelfx_ui)
target_link_libraries(pixelfx_ui PUBLIC pixelfx_services Qt6::Widgets Qt6::Svg)

set(_pixelfx_resources "${CMAKE_CURRENT_SOURCE_DIR}/../resources")
file(GLOB _pixelfx_ui_glyphs "${_pixelfx_resources}/icons/ui/*.svg")
qt_add_resources(pixelfx_ui "pixelfx_icons"
    PREFIX "/"
    BASE "${_pixelfx_resources}"
    FILES ${_pixelfx_ui_glyphs} "${_pixelfx_resources}/icons/pixelfx.png")
