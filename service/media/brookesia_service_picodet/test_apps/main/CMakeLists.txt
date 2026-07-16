idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        unity
        brookesia_lib_utils
        brookesia_service_helper
        brookesia_service_manager
        brookesia_service_picodet
        joltwallet__littlefs
    WHOLE_ARCHIVE TRUE
)

littlefs_create_partition_image(
    littlefs_data
    ../littlefs
    FLASH_IN_PROJECT
)
