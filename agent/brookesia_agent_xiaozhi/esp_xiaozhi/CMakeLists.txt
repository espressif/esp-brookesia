idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "include"
    PRIV_REQUIRES  esp_common nvs_flash app_update spi_flash json mbedtls mqtt esp_http_client esp_wifi
)
