idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE TRUE
)

if(CONFIG_BSP_USB_CONSOLE)
    # ESP-IDF's Unity runner bypasses stdio and talks to the ROM console directly.
    # Keep the workaround local to this test application so the Mosaico TinyUSB
    # console can be used for both the Unity menu and normal application logs.
    target_link_libraries(${COMPONENT_LIB} INTERFACE
        "-Wl,--wrap=unity_gets"
        "-Wl,--wrap=unity_putc"
        "-Wl,--wrap=unity_flush"
    )
endif()

if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP1_SSID})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        HAL_ADAPTOR_TEST_WIFI_SSID="$ENV{CI_TEST_WIFI_2_4G_AP1_SSID}"
    )
endif()

if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP1_PSW})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        HAL_ADAPTOR_TEST_WIFI_PSW="$ENV{CI_TEST_WIFI_2_4G_AP1_PSW}"
    )
endif()

littlefs_create_partition_image(littlefs_data ../littlefs FLASH_IN_PROJECT)
