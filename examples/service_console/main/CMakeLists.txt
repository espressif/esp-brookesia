file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_LIST_DIR}/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/*.c")
set(EMBEDDED_FILES "")

if (CONFIG_EXAMPLE_AGENTS_COZE_PRIVATE_KEY_FILE_PATH)
    if (NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/${CONFIG_EXAMPLE_AGENTS_COZE_PRIVATE_KEY_FILE_PATH})
        message(FATAL_ERROR "Coze Private Key file not found: ${CONFIG_EXAMPLE_AGENTS_COZE_PRIVATE_KEY_FILE_PATH}")
    endif()
    list(APPEND EMBEDDED_FILES ${CONFIG_EXAMPLE_AGENTS_COZE_PRIVATE_KEY_FILE_PATH})
    message(STATUS "Coze Private Key Path: ${CONFIG_EXAMPLE_AGENTS_COZE_PRIVATE_KEY_FILE_PATH}")
endif()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
    EMBED_TXTFILES ${EMBEDDED_FILES}
)

spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)

if (CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE)
    set(ASSETS_PARTITION_NAME "anim_icon")
    set(ASSETS_BIN_NAME ${ASSETS_PARTITION_NAME})

    if (CONFIG_BOARD_ESP_VOCAT_BOARD_V1_2)
        set(RESOLUTION "360_360")
    elseif (CONFIG_BOARD_ESP_BOX_3 OR CONFIG_BOARD_ESP32_S3_KORVO2_V3)
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

    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        ASSETS_PARTITION_NAME="${ASSETS_PARTITION_NAME}"
    )
endif()
