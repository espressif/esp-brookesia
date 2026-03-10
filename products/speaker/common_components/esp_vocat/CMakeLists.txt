idf_component_register(
    SRCS "esp_vocat.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES driver esp_lcd fatfs
)
