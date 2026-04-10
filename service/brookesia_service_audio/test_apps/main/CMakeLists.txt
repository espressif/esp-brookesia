idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity gen_bmgr_codes esp_psram
    WHOLE_ARCHIVE
)

spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)
