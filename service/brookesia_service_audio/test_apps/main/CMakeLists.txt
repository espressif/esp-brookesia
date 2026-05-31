idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE TRUE
)

littlefs_create_partition_image(littlefs_data ../littlefs FLASH_IN_PROJECT)
