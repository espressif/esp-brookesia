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
set(settings_stage_relative_paths)
if(CONFIG_BROOKESIA_GUI_BACKEND_GSP)
    # GSP reads res/i18n for the application name. The LVGL screens, templates,
    # images, and constants are not part of the GSP package.
    set(settings_stage_relative_paths RELATIVE_PATHS "res/i18n")
endif()
brookesia_stage_runtime_app_package(
    PACKAGE_ID "${app_settings_package_id}"
    SOURCE_DIR "${COMPONENT_DIR}/package"
    STAGE_ROOT "${app_settings_stage_root}"
    NO_INDEX
    ${settings_stage_relative_paths}
)
set(settings_device_resource_dir "${BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_APP_ROOT}/${app_settings_package_id}")
target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE
    "BROOKESIA_APP_SETTINGS_RESOURCE_DIR=\"${settings_device_resource_dir}\""
)

if(CONFIG_BROOKESIA_GUI_BACKEND_GSP)
    set(settings_strings "${CMAKE_CURRENT_BINARY_DIR}/settings_strings.hpp")
    set(settings_i18n_dir "${COMPONENT_DIR}/package/res/i18n")
    add_custom_command(
        OUTPUT "${settings_strings}"
        COMMAND "${PYTHON}" "${COMPONENT_DIR}/cmake/compile_strings.py"
            --source "${settings_i18n_dir}" --output "${settings_strings}"
        DEPENDS "${COMPONENT_DIR}/cmake/compile_strings.py"
            "${settings_i18n_dir}/en.json" "${settings_i18n_dir}/zh_CN.json"
        VERBATIM
    )
    target_sources(${COMPONENT_LIB} PRIVATE "${settings_strings}")
    target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    if(NOT COMMAND brookesia_gsp_add_bundle)
        message(FATAL_ERROR "brookesia_gui_gsp bundle helper is unavailable")
    endif()
    set(settings_scenes
        "${COMPONENT_DIR}/gsp/settings_home_1024.json"
        "${COMPONENT_DIR}/gsp/settings_my_device_1024.json"
        "${COMPONENT_DIR}/gsp/settings_display_1024.json"
        "${COMPONENT_DIR}/gsp/settings_wifi_1024.json"
        "${COMPONENT_DIR}/gsp/settings_wifi_connect_1024.json"
        "${COMPONENT_DIR}/gsp/settings_sound_1024.json"
        "${COMPONENT_DIR}/gsp/settings_language_1024.json"
        "${COMPONENT_DIR}/gsp/settings_theme_1024.json"
        "${COMPONENT_DIR}/gsp/settings_time_zone_1024.json"
    )
    brookesia_gsp_prepare_theme(settings_light_scenes light ${settings_scenes})
    brookesia_gsp_prepare_theme(settings_dark_scenes dark ${settings_scenes})
    brookesia_gsp_add_bundle(${COMPONENT_LIB}
        SCENES ${settings_light_scenes}
        PIXEL_FORMAT rgb565
        DYNAMIC_FONT "${COMPONENT_DIR}/gsp/fonts/DroidSansFallback-CommonChinese.ttf"
        SYMBOL settings
    )
    if(CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES)
        brookesia_gsp_compress_bundle(${COMPONENT_LIB} SYMBOL settings)
    endif()
    brookesia_gsp_add_bundle(${COMPONENT_LIB}
        SCENES ${settings_dark_scenes}
        PIXEL_FORMAT rgb565
        SYMBOL settings_dark
    )
    if(CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES)
        brookesia_gsp_compress_bundle(${COMPONENT_LIB} SYMBOL settings_dark)
    endif()
endif()

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})
