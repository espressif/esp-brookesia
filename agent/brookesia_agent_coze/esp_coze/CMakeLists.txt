set(public_include_dirs "./include")
idf_component_register(INCLUDE_DIRS "${public_include_dirs}"
                       SRCS "src/esp_coze_chat.c" "src/esp_coze_utils.c"
                       PRIV_REQUIRES esp_http_client esp_websocket_client json
                       WHOLE_ARCHIVE)
