set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

idf_component_register(
    SRCS
        "src/display/backlight_impl.cpp"
        "src/display/device.cpp"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
    REQUIRES
        brookesia_hal_adaptor
        esp_board_manager
        esp_lcd_co5300
)

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_DISPLAY_DEVICE)
    set(BROOKESIA_HAL_CUSTOM_DISPLAY_DEVICE_PLUGIN_SYMBOL custom_display_device_plugin_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_HAL_CUSTOM_DISPLAY_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_HAL_CUSTOM_DISPLAY_DEVICE_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_HAL_CUSTOM_DISPLAY_DEVICE_PLUGIN_SYMBOL}")
endif()
