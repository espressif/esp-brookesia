if(CONFIG_BROOKESIA_GUI_BACKEND_GSP AND CONFIG_BROOKESIA_GUI_BACKEND_LVGL)
    message(FATAL_ERROR "Select one Brookesia GUI backend, not both gsp and lvgl")
endif()

if(CONFIG_BROOKESIA_GUI_BACKEND_LVGL)
    set(SUPER_MAIN "${CMAKE_CURRENT_LIST_DIR}/../../../system/super/main")
    set(SRCS
        "${CMAKE_CURRENT_LIST_DIR}/main_lvgl.cpp"
        "${SUPER_MAIN}/modules/display.cpp"
        "${SUPER_MAIN}/modules/general_services.cpp"
    )
    set(EXTRA_INCLUDE_DIRS "${SUPER_MAIN}")
elseif(CONFIG_BROOKESIA_GUI_BACKEND_GSP)
    set(SRCS
        "${CMAKE_CURRENT_LIST_DIR}/main_gsp.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/launcher_app.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/launcher_dark.cpp"
    )
    if(CONFIG_BROOKESIA_GSP_WIFI_PROBE)
        list(APPEND SRCS "${CMAKE_CURRENT_LIST_DIR}/wifi_probe_hooks.cpp")
        set(probe_requires esp_event esp_wifi)
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

if(CONFIG_BROOKESIA_GSP_WIFI_PROBE)
    foreach(symbol IN ITEMS esp_wifi_scan_get_ap_num esp_wifi_scan_get_ap_records esp_event_post
            esp_gsp_component_set_text)
        target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--wrap=${symbol}")
    endforeach()
endif()

# Storage is a plugin. Reference its registrar so SystemCore can bind it.
target_link_libraries(${COMPONENT_LIB} PRIVATE "-u service_storage_symbol" "-u service_device_symbol")
target_link_libraries(${COMPONENT_LIB} PRIVATE "-u settings_audio_playback_symbol")

if(CONFIG_BROOKESIA_GUI_BACKEND_GSP)
    if(NOT COMMAND brookesia_gsp_add_bundle)
        message(FATAL_ERROR "brookesia_gui_gsp bundle helper is unavailable")
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/../../../../app/brookesia_app_settings/cmake/prepare_themes.cmake")
    set(launcher_scene "${CMAKE_CURRENT_LIST_DIR}/scenes/launcher_1024.json")
    settings_prepare_theme(launcher_light_scene light "${launcher_scene}")
    settings_prepare_theme(launcher_dark_scene dark "${launcher_scene}")
    brookesia_gsp_add_bundle(${COMPONENT_LIB}
        SCENES ${launcher_light_scene}
        PIXEL_FORMAT rgb565
        SYMBOL launcher
    )
    if(CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES)
        brookesia_gsp_compress_bundle(${COMPONENT_LIB} SYMBOL launcher)
    endif()
    brookesia_gsp_add_bundle(${COMPONENT_LIB}
        SCENES ${launcher_dark_scene}
        PIXEL_FORMAT rgb565
        SYMBOL launcher_dark
    )
    if(CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES)
        brookesia_gsp_compress_bundle(${COMPONENT_LIB} SYMBOL launcher_dark)
    endif()

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
    get_property(brookesia_runtime_app_stage_targets GLOBAL PROPERTY BROOKESIA_RUNTIME_APP_STAGE_TARGETS)
    if(brookesia_runtime_app_stage_targets)
        list(REMOVE_DUPLICATES brookesia_runtime_app_stage_targets)
    endif()
    get_property(brookesia_system_super_stage_targets GLOBAL PROPERTY BROOKESIA_SYSTEM_SUPER_STAGE_TARGETS)
    if(brookesia_system_super_stage_targets)
        list(REMOVE_DUPLICATES brookesia_system_super_stage_targets)
    endif()
    set(test_tone "${CMAKE_CURRENT_LIST_DIR}/../littlefs/settings-tone.wav")
    add_custom_command(OUTPUT "${test_tone}"
        COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/make_test_tone.py" "${test_tone}"
        DEPENDS "${CMAKE_CURRENT_LIST_DIR}/make_test_tone.py"
        VERBATIM
    )
    add_custom_target(settings_test_tone DEPENDS "${test_tone}")
    littlefs_create_partition_image(
        littlefs_data
        ../littlefs
        FLASH_IN_PROJECT
        DEPENDS settings_test_tone ${brookesia_runtime_app_stage_targets} ${brookesia_system_super_stage_targets}
    )
endif()
