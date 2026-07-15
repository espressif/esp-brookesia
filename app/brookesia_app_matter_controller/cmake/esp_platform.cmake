#
# ESP Platform
#
idf_component_register(
    SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    REQUIRES ${COMPONENT_REQUIRES}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_link_libraries(${COMPONENT_LIB} PUBLIC "-u app_matter_controller_provider_symbol")

set(app_matter_controller_package_id "brookesia.app.matter_controller")
idf_component_get_property(brookesia_system_core_dir brookesia_system_core COMPONENT_DIR)
include(${brookesia_system_core_dir}/cmake/runtime_app_stage.cmake)
brookesia_system_core_get_esp_runtime_app_stage_root(app_matter_controller_stage_root)
brookesia_stage_runtime_app_package(
    PACKAGE_ID "${app_matter_controller_package_id}"
    SOURCE_DIR "${COMPONENT_DIR}/package"
    STAGE_ROOT "${app_matter_controller_stage_root}"
    NO_INDEX
)
if(NOT COMMAND brookesia_gui_lvgl_pack_images)
    idf_build_get_property(build_components BUILD_COMPONENTS)
    set(brookesia_gui_lvgl_component "")
    if("brookesia_gui_lvgl" IN_LIST build_components)
        set(brookesia_gui_lvgl_component "brookesia_gui_lvgl")
    elseif("espressif__brookesia_gui_lvgl" IN_LIST build_components)
        set(brookesia_gui_lvgl_component "espressif__brookesia_gui_lvgl")
    endif()
    if(NOT brookesia_gui_lvgl_component)
        message(FATAL_ERROR "brookesia_app_matter_controller requires brookesia_gui_lvgl to stage image resources")
    endif()
    idf_component_get_property(brookesia_gui_lvgl_dir ${brookesia_gui_lvgl_component} COMPONENT_DIR)
    include(${brookesia_gui_lvgl_dir}/cmake/image_pack.cmake)
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

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})
