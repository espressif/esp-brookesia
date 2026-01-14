set(SRCS "main.cpp" "general_services.cpp" "ai_agents.cpp" "audio_service.cpp" "expression.cpp")
set(EMBEDDED_FILES "")

if (CONFIG_EXAMPLE_AGENTS_COZE_PRIVATE_KEY_FILE_PATH)
    list(APPEND EMBEDDED_FILES ${CONFIG_EXAMPLE_AGENTS_COZE_PRIVATE_KEY_FILE_PATH})
    message(STATUS "Coze Private Key Path: ${CONFIG_EXAMPLE_AGENTS_COZE_PRIVATE_KEY_FILE_PATH}")
endif()
if (CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER)
    list(APPEND SRCS "board.cpp")
endif()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
    EMBED_TXTFILES ${EMBEDDED_FILES}
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)

spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)

if (CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE)
    set(ASSETS_PARTITION_NAME "anim_icon")
    set(ASSETS_BIN_NAME "anim_icon")
    if (CONFIG_BOARD_ECHOEAR_CORE_BOARD_V1_2)
        set(RESOLUTION "360_360")
    elseif (CONFIG_BOARD_ESP_BOX_3)
        set(RESOLUTION "320_240")
    elseif (CONFIG_BOARD_ESP32_P4_FUNCTION_EV)
        set(RESOLUTION "1024_600")
    else()
        message(WARNING "Unsupported board, using default resolution 320_240")
        set(RESOLUTION "320_240")
    endif()
    message(STATUS "Asset Resolution: ${RESOLUTION}")

    set(ASSETS_FILE "${CMAKE_BINARY_DIR}/${ASSETS_BIN_NAME}.bin")
    build_speaker_assets_bin(${ASSETS_BIN_NAME} ${RESOLUTION} ${ASSETS_FILE} ${CONFIG_MMAP_FILE_NAME_LENGTH})
    message(STATUS "Generated emote assets: ${ASSETS_FILE} -> ${ASSETS_PARTITION_NAME} partition")
    esptool_py_flash_to_partition(flash ${ASSETS_PARTITION_NAME} "${ASSETS_FILE}")
endif()
