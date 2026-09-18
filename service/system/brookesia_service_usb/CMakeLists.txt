if(NOT ESP_PLATFORM)
    return()
endif()

set(COMPONENT_DIR ${CMAKE_CURRENT_LIST_DIR})

idf_component_register(
    SRCS
        src/protocol.cpp
        src/service_usb.cpp
        src/transport_serial_jtag.cpp
        src/transport_uart.cpp
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS src
    REQUIRES brookesia_service_helper
    PRIV_REQUIRES mbedtls esp_driver_uart esp_driver_usb_serial_jtag esp_hal_usb esp_hw_support
)

if(CONFIG_BROOKESIA_SERVICE_USB_ENABLE_AUTO_REGISTER)
    set(BROOKESIA_SERVICE_USB_PLUGIN_SYMBOL service_usb_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_SERVICE_USB_PLUGIN_SYMBOL=${BROOKESIA_SERVICE_USB_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_SERVICE_USB_PLUGIN_SYMBOL}")
endif()

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})
