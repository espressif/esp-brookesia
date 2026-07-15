#
# PC Platform
#
set(COMPONENT_LIB brookesia_system_super_impl)
set(component_pc_config_compile_definitions "")

if(NOT TARGET brookesia::lib_utils)
    add_subdirectory(
        ${COMPONENT_DIR}/../../utils/brookesia_lib_utils
        ${CMAKE_BINARY_DIR}/brookesia_lib_utils
    )
endif()
option(
    BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG on PC"
    OFF
)
option(
    BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_STARTUP_OVERLAY
    "Default value of CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_STARTUP_OVERLAY on PC"
    ON
)
option(
    BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_RESOURCE_FONT_COPY
    "Default value of CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_RESOURCE_FONT_COPY on PC"
    ON
)
set(
    BROOKESIA_SYSTEM_SUPER_PC_CONFIG_STARTUP_ROOT_JSON
    "startup/root.json"
    CACHE STRING
    "Default value of CONFIG_BROOKESIA_SYSTEM_SUPER_STARTUP_ROOT_JSON on PC"
)
option(
    BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_APP_LAUNCH_TRANSITION
    "Compatibility value of CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_TRANSITION on PC"
    OFF
)
set(
    BROOKESIA_SYSTEM_SUPER_PC_CONFIG_APP_LAUNCH_ROOT_JSON
    "app_launch/root.json"
    CACHE STRING
    "Compatibility value of CONFIG_BROOKESIA_SYSTEM_SUPER_APP_LAUNCH_ROOT_JSON on PC"
)
option(
    BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_APP_LAUNCH_ANIMATION
    "Default value of CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_ANIMATION on PC"
    ON
)
set(
    BROOKESIA_SYSTEM_SUPER_PC_CONFIG_APP_LAUNCH_POST_COMPLETE_HOLD_MS
    "0"
    CACHE STRING
    "Default value of CONFIG_BROOKESIA_SYSTEM_SUPER_APP_LAUNCH_POST_COMPLETE_HOLD_MS on PC"
)

if(BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG=0)
endif()
if(BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_STARTUP_OVERLAY)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_STARTUP_OVERLAY=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_STARTUP_OVERLAY=0)
endif()
if(BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_RESOURCE_FONT_COPY)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_RESOURCE_FONT_COPY=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_RESOURCE_FONT_COPY=0)
endif()
if(BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_APP_LAUNCH_TRANSITION)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_TRANSITION=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_TRANSITION=0)
endif()
if(BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_APP_LAUNCH_ANIMATION)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_ANIMATION=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_ANIMATION=0)
endif()
string(CONCAT
    system_super_pc_config_app_launch_post_complete_hold_def
    "CONFIG_BROOKESIA_SYSTEM_SUPER_APP_LAUNCH_POST_COMPLETE_HOLD_MS="
    "(${BROOKESIA_SYSTEM_SUPER_PC_CONFIG_APP_LAUNCH_POST_COMPLETE_HOLD_MS})"
)
list(APPEND component_pc_config_compile_definitions
    CONFIG_BROOKESIA_SYSTEM_SUPER_STARTUP_ROOT_JSON="${BROOKESIA_SYSTEM_SUPER_PC_CONFIG_STARTUP_ROOT_JSON}"
    CONFIG_BROOKESIA_SYSTEM_SUPER_APP_LAUNCH_ROOT_JSON="${BROOKESIA_SYSTEM_SUPER_PC_CONFIG_APP_LAUNCH_ROOT_JSON}"
    ${system_super_pc_config_app_launch_post_complete_hold_def}
)

if(NOT DEFINED BROOKESIA_SYSTEM_CORE_PC_STAGE_SYSTEM_ROOT)
    set(BROOKESIA_SYSTEM_CORE_PC_STAGE_SYSTEM_ROOT "${CMAKE_BINARY_DIR}/brookesia/system")
endif()

if(NOT TARGET brookesia::service_display)
    add_subdirectory(
        ${COMPONENT_DIR}/../../service/media/brookesia_service_display
        ${CMAKE_BINARY_DIR}/brookesia_service_display
    )
endif()
if(NOT TARGET brookesia::service_wifi)
    add_subdirectory(
        ${COMPONENT_DIR}/../../service/network/brookesia_service_wifi
        ${CMAKE_BINARY_DIR}/brookesia_service_wifi
    )
endif()
if(NOT TARGET brookesia::service_storage)
    add_subdirectory(
        ${COMPONENT_DIR}/../../service/system/brookesia_service_storage
        ${CMAKE_BINARY_DIR}/brookesia_service_storage
    )
endif()

include(${COMPONENT_DIR}/cmake/resource_stage.cmake)
brookesia_system_super_stage_resources(
    SYSTEM_STAGE_ROOT "${BROOKESIA_SYSTEM_CORE_PC_STAGE_SYSTEM_ROOT}"
    COPY_FONTS "${BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_RESOURCE_FONT_COPY}"
)

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)
brookesia_define_component_version(${COMPONENT_LIB} ${COMPONENT_DIR} BROOKESIA_SYSTEM_SUPER)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::system_core
    PRIVATE
        brookesia::lib_utils
        brookesia::service_display
        brookesia::service_storage
        brookesia::service_wifi
)
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        ${component_pc_config_compile_definitions}
)

if(DEFINED BROOKESIA_SYSTEM_SUPER_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_SYSTEM_SUPER_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SYSTEM_SUPER_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::system_super ALIAS ${COMPONENT_LIB})
