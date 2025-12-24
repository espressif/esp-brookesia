set(SRCS "main.cpp" "services.cpp" "agents_auth.cpp")
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
