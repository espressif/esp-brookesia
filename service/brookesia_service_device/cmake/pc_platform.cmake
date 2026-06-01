#
# PC Platform
#
set(COMPONENT_LIB brookesia_service_device_impl)
set(component_pc_config_compile_definitions "")

option(
    BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER
    "Default value of CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER on PC"
    ON
)
option(
    BROOKESIA_SERVICE_DEVICE_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG on PC"
    OFF
)

if(BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER)
    set(BROOKESIA_SERVICE_DEVICE_PLUGIN_SYMBOL service_device_symbol)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER=1)
    list(APPEND component_pc_config_compile_definitions
        BROOKESIA_SERVICE_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_SERVICE_DEVICE_PLUGIN_SYMBOL}
    )
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER=0)
endif()
if(BROOKESIA_SERVICE_DEVICE_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG=0)
endif()

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::service_helper
        brookesia::hal_interface
)
if(BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER)
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            "-u ${BROOKESIA_SERVICE_DEVICE_PLUGIN_SYMBOL}"
    )
endif()
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        ${component_pc_config_compile_definitions}
)

if(DEFINED BROOKESIA_SERVICE_DEVICE_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_SERVICE_DEVICE_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SERVICE_DEVICE_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_SERVICE_DEVICE_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_SERVICE_DEVICE_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SERVICE_DEVICE_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::service_device ALIAS ${COMPONENT_LIB})
