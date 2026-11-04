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

# Packed asset bundles: one assets.bundle per document root, generated into a staged
# copy of the resource tree so the source tree stays clean. The runtime then opens a
# single file per document instead of one file per JSON asset (an open costs ~5.25 ms
# on littlefs regardless of size).
# The app and system staging targets fill BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT
# at build time, so the bundle-off image source is that same staged tree.
set(SUPER_LITTLEFS_DIR "${BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT}")
if(CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_ASSET_BUNDLE)
    if(NOT PYTHON)
        set(PYTHON python3)
    endif()
    set(SUPER_ASSET_BUNDLE_SRC
        "${CMAKE_CURRENT_LIST_DIR}/../../../../tools/gui_asset_bundle/src")
    set(SUPER_LITTLEFS_STAGE "${CMAKE_CURRENT_BINARY_DIR}/littlefs_stage")
    file(REMOVE_RECURSE "${SUPER_LITTLEFS_STAGE}")
    file(COPY "${SUPER_LITTLEFS_DIR}/" DESTINATION "${SUPER_LITTLEFS_STAGE}")
    file(GLOB_RECURSE SUPER_DOC_ROOTS "${SUPER_LITTLEFS_STAGE}/*/root.json")
    foreach(doc_root ${SUPER_DOC_ROOTS})
        get_filename_component(doc_dir "${doc_root}" DIRECTORY)
        execute_process(
            # JSON only: this tree already carries ~2 MB of images and the 5 MB
            # littlefs partition has no room for a second copy. Products that want
            # image bundles should size the partition first.
            COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${SUPER_ASSET_BUNDLE_SRC}"
                    "${PYTHON}" -m brookesia_gui_asset_bundle.cli pack
                    --res-root "${doc_dir}" --out "${doc_dir}/assets.bundle"
                    --include "*.json"
            RESULT_VARIABLE bundle_status
            OUTPUT_VARIABLE bundle_output
            ERROR_VARIABLE bundle_error
        )
        if(NOT bundle_status EQUAL 0)
            message(FATAL_ERROR "super: packing ${doc_dir} failed: ${bundle_error}")
        endif()
        message(STATUS "super: ${bundle_output}")
    endforeach()
    set(SUPER_LITTLEFS_IMAGE_SRC "${SUPER_LITTLEFS_STAGE}")
else()
    set(SUPER_LITTLEFS_IMAGE_SRC "${SUPER_LITTLEFS_DIR}")
endif()

# `littlefs_create_partition_image` is only defined when the joltwallet/littlefs
# component is pulled into the build. That dependency is conditional (see
# brookesia_hal_adaptor: it requires the storage device + littlefs Kconfig options),
# so guard the call to avoid a hard configure error when littlefs is absent.
if(COMMAND littlefs_create_partition_image)
    littlefs_create_partition_image(
        littlefs_data
        "${SUPER_LITTLEFS_IMAGE_SRC}"
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
