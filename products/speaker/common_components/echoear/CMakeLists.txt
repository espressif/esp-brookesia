idf_component_register(
    SRCS "echoear.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES driver esp_lcd fatfs
)
