# app: identity, command line, single instance, the Application object.
configure_file(version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/app/version.h" @ONLY)

qt_add_library(pixelfx_app STATIC
    application.cpp
    application.h
    cli_options.cpp
    cli_options.h
    logging.cpp
    logging.h
    single_instance.cpp
    single_instance.h
)
pixelfx_set_warnings(pixelfx_app)
target_include_directories(pixelfx_app PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(pixelfx_app PUBLIC pixelfx_ui Qt6::Network)
