idf_component_register(
    SRCS "phone_common_net.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_netif esp_wifi freertos nvs_flash
)
