#
# PC Platform
#
set(COMPONENT_LIB brookesia_hal_interface_impl)
set(component_pc_config_compile_definitions "")

option(
    BROOKESIA_HAL_INTERFACE_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_HAL_INTERFACE_ENABLE_DEBUG_LOG on PC"
    OFF
)

if(BROOKESIA_HAL_INTERFACE_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_HAL_INTERFACE_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_HAL_INTERFACE_ENABLE_DEBUG_LOG=0)
endif()

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)
brookesia_define_component_version(${COMPONENT_LIB} ${COMPONENT_DIR} BROOKESIA_HAL_INTERFACE)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::lib_utils
)

target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        ${component_pc_config_compile_definitions}
)

if(DEFINED BROOKESIA_HAL_INTERFACE_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_HAL_INTERFACE_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_HAL_INTERFACE_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_HAL_INTERFACE_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_HAL_INTERFACE_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_HAL_INTERFACE_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::hal_interface ALIAS ${COMPONENT_LIB})
