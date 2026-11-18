file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_LIST_DIR}/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/*.c")
set(common_dir "${CMAKE_CURRENT_LIST_DIR}/../../common")
list(APPEND SRCS "${common_dir}/lvgl/display.cpp" "${common_dir}/lvgl/general_services.cpp")
set(EMBEDDED_FILES "")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "." "${common_dir}"
    EMBED_TXTFILES ${EMBEDDED_FILES}
)

get_property(brookesia_runtime_app_stage_targets GLOBAL PROPERTY BROOKESIA_RUNTIME_APP_STAGE_TARGETS)
if(brookesia_runtime_app_stage_targets)
    list(REMOVE_DUPLICATES brookesia_runtime_app_stage_targets)
endif()
get_property(brookesia_system_super_stage_targets GLOBAL PROPERTY BROOKESIA_SYSTEM_SUPER_STAGE_TARGETS)
if(brookesia_system_super_stage_targets)
    list(REMOVE_DUPLICATES brookesia_system_super_stage_targets)
endif()

# `littlefs_create_partition_image` is only defined when the joltwallet/littlefs
# component is pulled into the build. That dependency is conditional (see
# brookesia_hal_adaptor: it requires the storage device + littlefs Kconfig options),
# so guard the call to avoid a hard configure error when littlefs is absent.
if(COMMAND littlefs_create_partition_image)
    littlefs_create_partition_image(
        littlefs_data
        "${BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT}"
        FLASH_IN_PROJECT
        DEPENDS ${brookesia_runtime_app_stage_targets} ${brookesia_system_super_stage_targets}
    )
else()
    message(WARNING
        "littlefs_create_partition_image is not available (joltwallet/littlefs not in build); "
        "skipping littlefs_data image generation. Enable "
        "BROOKESIA_HAL_ADAPTOR_ENABLE_STORAGE_DEVICE / *_FILE_SYSTEM_IMPL / *_ENABLE_LITTLEFS "
        "if the littlefs partition is needed.")
endif()
