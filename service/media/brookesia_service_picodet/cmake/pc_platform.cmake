#
# PC Platform (stub: the esp-dl inference backend is unavailable off-device)
#
set(COMPONENT_LIB brookesia_service_picodet_impl)
set(component_pc_config_compile_definitions "")

option(
    BROOKESIA_SERVICE_PICODET_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SERVICE_PICODET_ENABLE_DEBUG_LOG on PC"
    OFF
)
if(NOT TARGET brookesia::service_helper)
    add_subdirectory(
        ${COMPONENT_DIR}/../../framework/brookesia_service_helper
        ${CMAKE_BINARY_DIR}/brookesia_service_helper
    )
endif()

if(BROOKESIA_SERVICE_PICODET_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_PICODET_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_PICODET_ENABLE_DEBUG_LOG=0)
endif()

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)
add_library(brookesia::service_picodet ALIAS ${COMPONENT_LIB})

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        BROOKESIA_SERVICE_PICODET_VER_MAJOR=${COMPONENT_VERSION_MAJOR}
        BROOKESIA_SERVICE_PICODET_VER_MINOR=${COMPONENT_VERSION_MINOR}
        BROOKESIA_SERVICE_PICODET_VER_PATCH=${COMPONENT_VERSION_PATCH}
        ${component_pc_config_compile_definitions}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::service_helper
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    BROOKESIA_SERVICE_PICODET_PLUGIN_SYMBOL=service_picodet_symbol
)
target_link_libraries(${COMPONENT_LIB} PUBLIC "-u service_picodet_symbol")

if(DEFINED BROOKESIA_SERVICE_PICODET_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_SERVICE_PICODET_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SERVICE_PICODET_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_SERVICE_PICODET_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_SERVICE_PICODET_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SERVICE_PICODET_PC_COMPILE_OPTIONS}
    )
endif()
