set(srcs)

if(CONFIG_BSP_USB_CONSOLE)
    list(APPEND srcs "src/esp_mosaico_usb_console.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES
        esp_hal_wdt
        esp_hw_support
        esp_system
        esp_timer
        esp_tinyusb
        soc
)

if(CONFIG_BSP_USB_CONSOLE_AUTO_INIT)
    # The component has no application-side reference when auto-init is used.
    idf_component_set_property(${COMPONENT_NAME} WHOLE_ARCHIVE TRUE)
endif()
