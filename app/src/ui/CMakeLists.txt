# ui: widgets. Tokens come from DOCS/DESIGN.md section 1; screens follow the approved mocks.
qt_add_library(pixelfx_ui STATIC
    about_sheet.cpp
    about_sheet.h
    adjust_panel.cpp
    adjust_panel.h
    batch_page.cpp
    batch_page.h
    canvas.cpp
    canvas.h
    crop_overlay.h
    crop_panel.cpp
    crop_panel.h
    color_wheel.cpp
    color_wheel.h
    curve_editor.cpp
    curve_editor.h
    export_sheet.cpp
    filmstrip.cpp
    filmstrip.h
    export_sheet.h
    histogram_view.cpp
    histogram_view.h
    icons.cpp
    icons.h
    look_editor.cpp
    look_editor.h
    looks_panel.cpp
    looks_panel.h
    main_window.cpp
    main_window.h
    paste_sheet.cpp
    paste_sheet.h
    presets_panel.cpp
    presets_panel.h
    preset_save_sheet.cpp
    preset_save_sheet.h
    settings_sheet.cpp
    settings_sheet.h
    shortcuts_sheet.cpp
    shortcuts_sheet.h
    style.cpp
    style.h
    straighten_dial.cpp
    straighten_dial.h
    watermark_sheet.cpp
    watermark_sheet.h
    welcome_page.cpp
    whats_new_sheet.cpp
    whats_new_sheet.h
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
