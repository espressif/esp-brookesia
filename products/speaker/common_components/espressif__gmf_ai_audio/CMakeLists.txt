set(srcs "./src/esp_gmf_wn.c")

if((${IDF_TARGET} STREQUAL "esp32s3") OR (${IDF_TARGET} STREQUAL "esp32p4") OR (${IDF_TARGET} STREQUAL "esp32"))
    list(APPEND srcs "./src/esp_gmf_afe_manager.c" "./src/esp_gmf_afe.c" "./src/esp_gmf_aec.c")
endif()

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "./include"
    PRIV_INCLUDE_DIRS "./private_include"
    REQUIRES esp-sr esp_timer
)
