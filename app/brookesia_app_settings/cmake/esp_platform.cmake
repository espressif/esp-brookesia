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
target_link_libraries(${COMPONENT_LIB} PUBLIC "-u app_settings_provider_symbol")

set(app_settings_package_id "brookesia.general.settings")
if(DEFINED BROOKESIA_SYSTEM_CORE_COMPONENT_DIR AND NOT "${BROOKESIA_SYSTEM_CORE_COMPONENT_DIR}" STREQUAL "")
    set(brookesia_system_core_dir "${BROOKESIA_SYSTEM_CORE_COMPONENT_DIR}")
else()
    idf_build_get_property(build_components BUILD_COMPONENTS)
    if("espressif__brookesia_system_core" IN_LIST build_components)
        idf_component_get_property(brookesia_system_core_dir espressif__brookesia_system_core COMPONENT_DIR)
    else()
        idf_component_get_property(brookesia_system_core_dir brookesia_system_core COMPONENT_DIR)
    endif()
endif()
include("${brookesia_system_core_dir}/cmake/runtime_app_stage.cmake")
brookesia_system_core_get_esp_runtime_app_stage_root(app_settings_stage_root)
brookesia_stage_runtime_app_package(
    PACKAGE_ID "${app_settings_package_id}"
    SOURCE_DIR "${COMPONENT_DIR}/package"
    STAGE_ROOT "${app_settings_stage_root}"
    NO_INDEX
)

if(CONFIG_BROOKESIA_GUI_BACKEND_GSP)
    if(NOT COMMAND brookesia_gsp_add_bundle)
        message(FATAL_ERROR "brookesia_gui_gsp bundle helper is unavailable")
    endif()
    brookesia_gsp_add_bundle(${COMPONENT_LIB}
        SCENES
            "${COMPONENT_DIR}/gsp/settings_home_1024.json"
            "${COMPONENT_DIR}/gsp/settings_display_1024.json"
            "${COMPONENT_DIR}/gsp/settings_wifi_1024.json"
            "${COMPONENT_DIR}/gsp/settings_wifi_connect_1024.json"
            "${COMPONENT_DIR}/gsp/settings_keyboard_1024.json"
        PIXEL_FORMAT rgb565
        DYNAMIC_FONT "${COMPONENT_DIR}/gsp/fonts/DroidSansFallback-CommonChinese.ttf"
        SYMBOL settings
    )
endif()

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})
