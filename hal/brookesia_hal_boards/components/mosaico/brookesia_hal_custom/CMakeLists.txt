set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_SRCS
    "src/board/audio_codec.c"
    "src/board/board_power.c"
    "src/board/bq27220_fuel_gauge.c"
    "src/board/camera_slot_claim.cpp"
    "src/board/device_cleanup.cpp"
    "src/board/display.c"
    "src/board/expansion_module_manager.c"
    "src/board/expansion_runtime_pin.c"
    "src/board/fs_nand.c"
    "src/display/backlight_impl.cpp"
    "src/display/device.cpp"
)
set(COMPONENT_REQUIRES
    brookesia_hal_adaptor
    esp_board_manager
    esp_lcd_co5300
    esp_driver_gpio
    esp_blockdev
)

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES)
    list(APPEND COMPONENT_SRCS
        "src/expansion/module_provider.cpp"
        "src/expansion/mosaico_module_manager.c"
    )
    list(APPEND COMPONENT_REQUIRES
        esp_driver_gpio
        esp_driver_i2c
        esp_hal_usb
    )
endif()

idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
    REQUIRES ${COMPONENT_REQUIRES}
    PRIV_REQUIRES
        esp_driver_i2c
        esp_driver_ledc
        esp_driver_spi
        spi_nand_flash
)

set(MOSAICO_DEVICE_CLEANUP_PLUGIN_SYMBOL mosaico_device_cleanup_plugin_symbol)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    MOSAICO_DEVICE_CLEANUP_PLUGIN_SYMBOL=${MOSAICO_DEVICE_CLEANUP_PLUGIN_SYMBOL}
)
target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${MOSAICO_DEVICE_CLEANUP_PLUGIN_SYMBOL}")

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_DISPLAY_DEVICE)
    set(BROOKESIA_HAL_CUSTOM_DISPLAY_DEVICE_PLUGIN_SYMBOL custom_display_device_plugin_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_HAL_CUSTOM_DISPLAY_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_HAL_CUSTOM_DISPLAY_DEVICE_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_HAL_CUSTOM_DISPLAY_DEVICE_PLUGIN_SYMBOL}")
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES)
    set(MOSAICO_EXPANSION_PROVIDER_PLUGIN_SYMBOL mosaico_expansion_provider_plugin_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        MOSAICO_EXPANSION_PROVIDER_PLUGIN_SYMBOL=${MOSAICO_EXPANSION_PROVIDER_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE
        "-u ${MOSAICO_EXPANSION_PROVIDER_PLUGIN_SYMBOL}"
    )
endif()
