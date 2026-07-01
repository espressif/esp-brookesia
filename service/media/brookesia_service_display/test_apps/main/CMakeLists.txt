idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        unity
        brookesia_lib_utils
        brookesia_hal_interface
        brookesia_service_display
        brookesia_service_helper
        brookesia_service_manager
    WHOLE_ARCHIVE TRUE
)
