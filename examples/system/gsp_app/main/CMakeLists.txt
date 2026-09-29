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
    )
    if(CONFIG_BROOKESIA_GSP_WIFI_PROBE)
        list(APPEND SRCS "${CMAKE_CURRENT_LIST_DIR}/wifi_probe_hooks.cpp")
        set(probe_requires esp_event esp_wifi)
    endif()
else()
    # The first dependency scan runs before sdkconfig exists.
    set(SRCS "${CMAKE_CURRENT_LIST_DIR}/main_pending.cpp")
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

if(CONFIG_BROOKESIA_GUI_BACKEND_GSP)
    if(NOT COMMAND brookesia_gsp_add_bundle)
        message(FATAL_ERROR "brookesia_gui_gsp bundle helper is unavailable")
    endif()
    brookesia_gsp_add_bundle(${COMPONENT_LIB}
        SCENES
            "${CMAKE_CURRENT_LIST_DIR}/scenes/launcher_1024.json"
        PIXEL_FORMAT rgb565
        SYMBOL launcher
    )

    if(NOT TARGET esp_gsp_project_config)
        message(FATAL_ERROR "esp-gsp project config target is unavailable")
    endif()
    set(gsp_dma2d_compat "${CMAKE_CURRENT_LIST_DIR}/../gsp_dma2d_idf60.h")
    target_compile_options(
        esp_gsp_project_config PRIVATE
        "SHELL:-include ${gsp_dma2d_compat}"
    )
endif()

if(CONFIG_BROOKESIA_GUI_BACKEND_LVGL)
    if(NOT COMMAND littlefs_create_partition_image)
        message(FATAL_ERROR
            "The LVGL desktop needs the littlefs_data image, but joltwallet/littlefs is not in the build")
    endif()
    get_property(brookesia_runtime_app_stage_targets GLOBAL PROPERTY BROOKESIA_RUNTIME_APP_STAGE_TARGETS)
    if(brookesia_runtime_app_stage_targets)
        list(REMOVE_DUPLICATES brookesia_runtime_app_stage_targets)
    endif()
    get_property(brookesia_system_super_stage_targets GLOBAL PROPERTY BROOKESIA_SYSTEM_SUPER_STAGE_TARGETS)
    if(brookesia_system_super_stage_targets)
        list(REMOVE_DUPLICATES brookesia_system_super_stage_targets)
    endif()
    littlefs_create_partition_image(
        littlefs_data
        ../littlefs
        FLASH_IN_PROJECT
        DEPENDS ${brookesia_runtime_app_stage_targets} ${brookesia_system_super_stage_targets}
    )
endif()
