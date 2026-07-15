#
# PC Platform
#
set(COMPONENT_LIB brookesia_app_matter_controller_impl)

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::system_core
        "-u app_matter_controller_provider_symbol"
)
if(TARGET brookesia::gui_lvgl)
    target_link_libraries(${COMPONENT_LIB}
        PRIVATE
            brookesia::gui_lvgl
    )
endif()
set(app_matter_controller_package_id "brookesia.app.matter_controller")
include(${BROOKESIA_SYSTEM_CORE_COMPONENT_DIR}/cmake/runtime_app_stage.cmake)
if(NOT DEFINED BROOKESIA_SYSTEM_CORE_PC_CONFIG_APP_ROOT)
    set(BROOKESIA_SYSTEM_CORE_PC_CONFIG_APP_ROOT "${CMAKE_BINARY_DIR}/brookesia/apps")
endif()
set(app_matter_controller_stage_root "${BROOKESIA_SYSTEM_CORE_PC_CONFIG_APP_ROOT}")
brookesia_stage_runtime_app_package(
    PACKAGE_ID "${app_matter_controller_package_id}"
    SOURCE_DIR "${COMPONENT_DIR}/package"
    STAGE_ROOT "${app_matter_controller_stage_root}"
    NO_INDEX
)
if(NOT COMMAND brookesia_gui_lvgl_pack_images)
    message(FATAL_ERROR "brookesia_app_matter_controller requires brookesia_gui_lvgl_pack_images() to stage image resources")
endif()
set(app_matter_controller_image_output_dir
    "${app_matter_controller_stage_root}/${app_matter_controller_package_id}/res/images"
)
# Remove stale packed assets when source images are deleted or renamed.
file(REMOVE_RECURSE "${app_matter_controller_image_output_dir}")
brookesia_gui_lvgl_pack_images(
    INPUT_DIR "${COMPONENT_DIR}/assets/images"
    OUTPUT_DIR "${app_matter_controller_image_output_dir}"
    IMAGE_FILES "${COMPONENT_DIR}/assets/images/matter_controller.png"
    CF RGB565A8
)

target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        BROOKESIA_APP_MATTER_CONTROLLER_RESOURCE_DIR="${app_matter_controller_stage_root}/${app_matter_controller_package_id}"
)

add_library(brookesia::app_matter_controller ALIAS ${COMPONENT_LIB})
