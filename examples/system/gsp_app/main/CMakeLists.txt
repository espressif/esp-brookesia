if(CONFIG_BROOKESIA_GUI_BACKEND_GSP AND CONFIG_BROOKESIA_GUI_BACKEND_LVGL)
    message(FATAL_ERROR "Select one Brookesia GUI backend, not both gsp and lvgl")
endif()

if(CONFIG_BROOKESIA_GUI_BACKEND_LVGL)
    set(common_dir "${CMAKE_CURRENT_LIST_DIR}/../../common")
    set(SRCS
        "${CMAKE_CURRENT_LIST_DIR}/main_lvgl.cpp"
        "${common_dir}/lvgl/display.cpp"
        "${common_dir}/lvgl/general_services.cpp"
    )
    set(EXTRA_INCLUDE_DIRS "${common_dir}")
elseif(CONFIG_BROOKESIA_GUI_BACKEND_GSP)
    set(SRCS
        "${CMAKE_CURRENT_LIST_DIR}/main_gsp.cpp"
    )
    if(CONFIG_BROOKESIA_GSP_WIFI_PROBE)
        list(APPEND SRCS "${CMAKE_CURRENT_LIST_DIR}/wifi_probe_hooks.cpp")
        set(probe_requires esp_event esp_wifi)
    endif()
    if(CONFIG_BROOKESIA_GSP_DELIVERY_PROBE)
        list(APPEND SRCS "${CMAKE_CURRENT_LIST_DIR}/delivery_probe.cpp")
    endif()
else()
    # The first dependency scan runs before sdkconfig exists.
    set(SRCS "${CMAKE_CURRENT_LIST_DIR}/main_pending.cpp")
endif()

if(CONFIG_BROOKESIA_GSP_WIFI_CONNECT_PROBE OR CONFIG_BROOKESIA_LVGL_WIFI_CONNECT_PROBE
        OR CONFIG_BROOKESIA_GSP_SETTINGS_PROBE)
    list(APPEND probe_requires esp_driver_usb_serial_jtag)
endif()

if(CONFIG_BROOKESIA_GUI_BACKEND_GSP OR CONFIG_BROOKESIA_GUI_BACKEND_LVGL)
    list(APPEND SRCS "${CMAKE_CURRENT_LIST_DIR}/audio_service.cpp")
endif()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "." ${EXTRA_INCLUDE_DIRS}
    PRIV_REQUIRES ${probe_requires}
)

if(CONFIG_BROOKESIA_LVGL_HW_PROBE)
    idf_component_optional_requires(PRIVATE lvgl lvgl__lvgl esp_lcd)
endif()

if(CONFIG_BROOKESIA_GSP_DELIVERY_PROBE)
    target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--wrap=esp_lcd_panel_draw_bitmap")
endif()

if(CONFIG_BROOKESIA_GSP_WIFI_PROBE)
    foreach(symbol IN ITEMS esp_wifi_scan_get_ap_num esp_wifi_scan_get_ap_records esp_event_post
            esp_gsp_component_set_text)
        target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--wrap=${symbol}")
    endforeach()
endif()

# Storage is a plugin. Reference its registrar so SystemCore can bind it.
target_link_libraries(${COMPONENT_LIB} PRIVATE "-u service_storage_symbol" "-u service_device_symbol")
if(CONFIG_BROOKESIA_GUI_BACKEND_GSP)
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u runtime_js_backend_symbol")
endif()
target_link_libraries(${COMPONENT_LIB} PRIVATE "-u settings_audio_playback_symbol")

if(CONFIG_BROOKESIA_GUI_BACKEND_GSP)
    if(NOT TARGET esp_gsp_project_config)
        message(FATAL_ERROR "esp-gsp project config target is unavailable")
    endif()
    set(gsp_dma2d_compat "${CMAKE_CURRENT_LIST_DIR}/../gsp_dma2d_idf60.h")
    target_compile_options(
        esp_gsp_project_config PRIVATE
        "SHELL:-include ${gsp_dma2d_compat}"
    )
endif()

if(CONFIG_BROOKESIA_GUI_BACKEND_GSP OR CONFIG_BROOKESIA_GUI_BACKEND_LVGL)
    if(NOT COMMAND littlefs_create_partition_image)
        message(FATAL_ERROR
            "This example needs the littlefs_data image, but joltwallet/littlefs is not in the build")
    endif()
    if(DEFINED BROOKESIA_SYSTEM_CORE_COMPONENT_DIR AND NOT "${BROOKESIA_SYSTEM_CORE_COMPONENT_DIR}" STREQUAL "")
        set(brookesia_system_core_dir "${BROOKESIA_SYSTEM_CORE_COMPONENT_DIR}")
    else()
        idf_build_get_property(brookesia_gsp_build_components BUILD_COMPONENTS)
        if("espressif__brookesia_system_core" IN_LIST brookesia_gsp_build_components)
            idf_component_get_property(brookesia_system_core_dir espressif__brookesia_system_core COMPONENT_DIR)
        else()
            idf_component_get_property(brookesia_system_core_dir brookesia_system_core COMPONENT_DIR)
        endif()
    endif()
    include("${brookesia_system_core_dir}/cmake/runtime_app_stage.cmake")
    brookesia_system_core_runtime_path_to_stage_dir(
        littlefs_image_dir
        "${BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_INTERNAL_ROOT}"
    )
    get_property(brookesia_runtime_app_stage_targets GLOBAL PROPERTY BROOKESIA_RUNTIME_APP_STAGE_TARGETS)
    if(brookesia_runtime_app_stage_targets)
        list(REMOVE_DUPLICATES brookesia_runtime_app_stage_targets)
    endif()
    get_property(brookesia_system_super_stage_targets GLOBAL PROPERTY BROOKESIA_SYSTEM_SUPER_STAGE_TARGETS)
    if(brookesia_system_super_stage_targets)
        list(REMOVE_DUPLICATES brookesia_system_super_stage_targets)
    endif()
    set(littlefs_image_depends settings_test_tone ${brookesia_runtime_app_stage_targets} ${brookesia_system_super_stage_targets})
    if(CONFIG_BROOKESIA_GUI_BACKEND_GSP)
        # system/super and system/fonts belong to the LVGL shell. Drop them even when
        # this build directory previously configured the LVGL backend.
        add_custom_target(brookesia_gsp_drop_lvgl_system_resources ALL
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${littlefs_image_dir}/system"
            COMMENT "Removing LVGL system resources from the GSP littlefs stage"
            VERBATIM
        )
        if(brookesia_system_super_stage_targets)
            add_dependencies(brookesia_gsp_drop_lvgl_system_resources ${brookesia_system_super_stage_targets})
        endif()
        list(APPEND littlefs_image_depends brookesia_gsp_drop_lvgl_system_resources)
    endif()
    set(test_tone "${littlefs_image_dir}/settings-tone.wav")
    add_custom_command(OUTPUT "${test_tone}"
        COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/make_test_tone.py" "${test_tone}"
        DEPENDS "${CMAKE_CURRENT_LIST_DIR}/make_test_tone.py"
        VERBATIM
    )
    add_custom_target(settings_test_tone DEPENDS "${test_tone}")
    littlefs_create_partition_image(
        littlefs_data
        "${littlefs_image_dir}"
        FLASH_IN_PROJECT
        DEPENDS ${littlefs_image_depends}
    )
endif()
