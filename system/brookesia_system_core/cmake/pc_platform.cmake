#
# PC Platform
#
set(COMPONENT_LIB brookesia_system_core_impl)
set(component_pc_config_compile_definitions "")

if(EMSCRIPTEN)
    if(NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB INTERFACE IMPORTED)
    endif()
    set_property(TARGET ZLIB::ZLIB APPEND PROPERTY
        INTERFACE_COMPILE_OPTIONS -sUSE_ZLIB=1
    )
    set_property(TARGET ZLIB::ZLIB APPEND PROPERTY
        INTERFACE_LINK_OPTIONS -sUSE_ZLIB=1
    )
else()
    find_package(ZLIB REQUIRED)
endif()

option(
    BROOKESIA_SYSTEM_CORE_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SYSTEM_CORE_ENABLE_DEBUG_LOG on PC"
    OFF
)
option(
    BROOKESIA_SYSTEM_CORE_PC_CONFIG_SYSTEM_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_SYSTEM_CORE_PC_CONFIG_SERVICE_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SYSTEM_CORE_SERVICE_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_SYSTEM_CORE_PC_CONFIG_RUNTIME_ASYNC_DEBUG_PROBE
    "Default value of CONFIG_BROOKESIA_SYSTEM_CORE_RUNTIME_ASYNC_DEBUG_PROBE on PC"
    OFF
)
set(
    BROOKESIA_SYSTEM_CORE_PC_STAGE_INTERNAL_ROOT
    "${CMAKE_BINARY_DIR}/brookesia"
    CACHE PATH
    "PC build staging root for the Android-like internal storage volume"
)
set(
    BROOKESIA_SYSTEM_CORE_PC_STAGE_APP_ROOT
    "${BROOKESIA_SYSTEM_CORE_PC_STAGE_INTERNAL_ROOT}/apps"
    CACHE PATH
    "PC build staging root for internal/apps"
)
set(
    BROOKESIA_SYSTEM_CORE_PC_STAGE_SYSTEM_ROOT
    "${BROOKESIA_SYSTEM_CORE_PC_STAGE_INTERNAL_ROOT}/system"
    CACHE PATH
    "PC build staging root for internal/system"
)
set(
    BROOKESIA_SYSTEM_CORE_PC_CONFIG_RUNTIME_APP_MAX_Z_ORDER
    100
    CACHE STRING
    "Default value of CONFIG_BROOKESIA_SYSTEM_CORE_RUNTIME_APP_MAX_Z_ORDER on PC"
)

if(BROOKESIA_SYSTEM_CORE_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_CORE_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_CORE_ENABLE_DEBUG_LOG=0)
endif()
if(BROOKESIA_SYSTEM_CORE_PC_CONFIG_SYSTEM_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG=0)
endif()
if(BROOKESIA_SYSTEM_CORE_PC_CONFIG_SERVICE_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_CORE_SERVICE_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_CORE_SERVICE_ENABLE_DEBUG_LOG=0)
endif()
if(BROOKESIA_SYSTEM_CORE_PC_CONFIG_RUNTIME_ASYNC_DEBUG_PROBE)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_CORE_RUNTIME_ASYNC_DEBUG_PROBE=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SYSTEM_CORE_RUNTIME_ASYNC_DEBUG_PROBE=0)
endif()
list(APPEND component_pc_config_compile_definitions
    CONFIG_BROOKESIA_SYSTEM_CORE_RUNTIME_APP_MAX_Z_ORDER=${BROOKESIA_SYSTEM_CORE_PC_CONFIG_RUNTIME_APP_MAX_Z_ORDER}
)

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
        brookesia::lib_utils
        brookesia::service_manager
        brookesia::service_helper
        brookesia::runtime_manager
        brookesia::gui_interface
        ZLIB::ZLIB
)
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        "BROOKESIA_SYSTEM_CORE_VER_MAJOR=(${COMPONENT_VERSION_MAJOR})"
        "BROOKESIA_SYSTEM_CORE_VER_MINOR=(${COMPONENT_VERSION_MINOR})"
        "BROOKESIA_SYSTEM_CORE_VER_PATCH=(${COMPONENT_VERSION_PATCH})"
        ${component_pc_config_compile_definitions}
)

if(DEFINED BROOKESIA_SYSTEM_CORE_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_SYSTEM_CORE_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SYSTEM_CORE_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_SYSTEM_CORE_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_SYSTEM_CORE_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SYSTEM_CORE_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::system_core ALIAS ${COMPONENT_LIB})
