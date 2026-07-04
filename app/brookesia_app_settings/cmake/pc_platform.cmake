#
# PC Platform
#
set(COMPONENT_LIB brookesia_app_settings_impl)
set(
    BROOKESIA_APP_SETTINGS_PC_CONFIG_WIFI_LIST_PAGE_SIZE
    5
    CACHE STRING
    "Number of Wi-Fi AP rows shown per Settings page on PC"
)
option(
    BROOKESIA_APP_SETTINGS_PC_CONFIG_ENABLE_MEMORY_TRACE
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_ENABLE_MEMORY_TRACE on PC (heap trace profiling)"
    OFF
)

if(NOT EMSCRIPTEN AND NOT TARGET brookesia::service_sntp)
    add_subdirectory(
        ${COMPONENT_DIR}/../../service/network/brookesia_service_sntp
        ${CMAKE_BINARY_DIR}/brookesia_service_sntp
    )
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
set(app_settings_public_links
    brookesia::system_core
    brookesia::service_device
    brookesia::service_storage
    brookesia::service_wifi
    brookesia::hal_interface
    "-u app_settings_provider_symbol"
)
if(TARGET brookesia::service_sntp)
    list(APPEND app_settings_public_links brookesia::service_sntp)
endif()
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        ${app_settings_public_links}
)
if(TARGET brookesia::gui_lvgl)
    target_link_libraries(${COMPONENT_LIB}
        PRIVATE
            brookesia::gui_lvgl
    )
endif()
if(BROOKESIA_APP_SETTINGS_PC_CONFIG_ENABLE_MEMORY_TRACE)
    set(BROOKESIA_APP_SETTINGS_PC_MEMORY_TRACE_DEF CONFIG_BROOKESIA_APP_SETTINGS_ENABLE_MEMORY_TRACE=1)
else()
    set(BROOKESIA_APP_SETTINGS_PC_MEMORY_TRACE_DEF CONFIG_BROOKESIA_APP_SETTINGS_ENABLE_MEMORY_TRACE=0)
endif()

target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        "BROOKESIA_APP_SETTINGS_VER_MAJOR=(${COMPONENT_VERSION_MAJOR})"
        "BROOKESIA_APP_SETTINGS_VER_MINOR=(${COMPONENT_VERSION_MINOR})"
        "BROOKESIA_APP_SETTINGS_VER_PATCH=(${COMPONENT_VERSION_PATCH})"
        BROOKESIA_APP_SETTINGS_PACKAGE_DIR="${COMPONENT_DIR}/package"
        "BROOKESIA_APP_SETTINGS_WIFI_MAX_VISIBLE_APS=(${BROOKESIA_APP_SETTINGS_PC_CONFIG_WIFI_LIST_PAGE_SIZE})"
        ${BROOKESIA_APP_SETTINGS_PC_MEMORY_TRACE_DEF}
)

set(app_settings_package_id "brookesia.general.settings")
include(${BROOKESIA_SYSTEM_CORE_COMPONENT_DIR}/cmake/runtime_app_stage.cmake)
if(NOT DEFINED BROOKESIA_SYSTEM_CORE_PC_STAGE_APP_ROOT)
    set(BROOKESIA_SYSTEM_CORE_PC_STAGE_APP_ROOT "${CMAKE_BINARY_DIR}/brookesia/apps")
endif()
set(app_settings_stage_root "${BROOKESIA_SYSTEM_CORE_PC_STAGE_APP_ROOT}")
get_filename_component(app_settings_stage_root_abs "${app_settings_stage_root}" ABSOLUTE)
set(app_settings_stage_dir "${app_settings_stage_root_abs}/${app_settings_package_id}")
brookesia_stage_runtime_app_package(
    PACKAGE_ID "${app_settings_package_id}"
    SOURCE_DIR "${COMPONENT_DIR}/package"
    STAGE_ROOT "${app_settings_stage_root}"
    NO_INDEX
)
string(MD5 app_settings_stage_target_hash "${app_settings_stage_dir}")
string(MAKE_C_IDENTIFIER "${app_settings_package_id}" app_settings_stage_target_id)
set(
    app_settings_stage_target
    "brookesia_stage_runtime_app_${app_settings_stage_target_id}_${app_settings_stage_target_hash}"
)

if(DEFINED BROOKESIA_APP_SETTINGS_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_APP_SETTINGS_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_APP_SETTINGS_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::app_settings ALIAS ${COMPONENT_LIB})
