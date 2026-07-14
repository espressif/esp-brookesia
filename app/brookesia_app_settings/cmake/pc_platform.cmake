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
option(
    BROOKESIA_APP_SETTINGS_PC_CONFIG_ENABLE_PRELOAD_DOM
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_ENABLE_PRELOAD_DOM on PC"
    ON
)
set(app_settings_pc_debug_config_compile_definitions "")
macro(app_settings_add_pc_debug_config name default_value description)
    set(config_var "BROOKESIA_APP_SETTINGS_PC_CONFIG_${name}")
    set(${config_var} "${default_value}" CACHE STRING "${description}")
    list(APPEND app_settings_pc_debug_config_compile_definitions
        "CONFIG_BROOKESIA_APP_SETTINGS_${name}=(${${config_var}})"
    )
endmacro()

app_settings_add_pc_debug_config(
    DEBUG_MEMORY_SAMPLE_INTERVAL_MIN_MS
    500
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_SAMPLE_INTERVAL_MIN_MS on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_SAMPLE_INTERVAL_DEFAULT_MS
    1000
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_SAMPLE_INTERVAL_DEFAULT_MS on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_SAMPLE_INTERVAL_MAX_MS
    10000
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_SAMPLE_INTERVAL_MAX_MS on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_INTERNAL_FREE_PERCENT_THRESHOLD_MIN
    1
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_FREE_PERCENT_THRESHOLD_MIN on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_INTERNAL_FREE_PERCENT_THRESHOLD_DEFAULT
    10
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_FREE_PERCENT_THRESHOLD_DEFAULT on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_INTERNAL_FREE_PERCENT_THRESHOLD_MAX
    80
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_FREE_PERCENT_THRESHOLD_MAX on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_INTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_MIN_KB
    1
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_MIN_KB on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_INTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_DEFAULT_KB
    10
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_DEFAULT_KB on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_INTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_MAX_KB
    512
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_MAX_KB on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_EXTERNAL_FREE_PERCENT_THRESHOLD_MIN
    1
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_FREE_PERCENT_THRESHOLD_MIN on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_EXTERNAL_FREE_PERCENT_THRESHOLD_DEFAULT
    20
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_FREE_PERCENT_THRESHOLD_DEFAULT on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_EXTERNAL_FREE_PERCENT_THRESHOLD_MAX
    80
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_FREE_PERCENT_THRESHOLD_MAX on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_EXTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_MIN_KB
    1
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_MIN_KB on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_EXTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_DEFAULT_KB
    500
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_DEFAULT_KB on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_MEMORY_EXTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_MAX_KB
    4096
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_MAX_KB on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_THREAD_PROFILING_INTERVAL_MIN_MS
    1000
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_THREAD_PROFILING_INTERVAL_MIN_MS on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_THREAD_PROFILING_INTERVAL_DEFAULT_MS
    5000
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_THREAD_PROFILING_INTERVAL_DEFAULT_MS on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_THREAD_PROFILING_INTERVAL_MAX_MS
    20000
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_THREAD_PROFILING_INTERVAL_MAX_MS on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_THREAD_SAMPLING_DURATION_MIN_MS
    100
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_THREAD_SAMPLING_DURATION_MIN_MS on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_THREAD_SAMPLING_DURATION_DEFAULT_MS
    1000
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_THREAD_SAMPLING_DURATION_DEFAULT_MS on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_THREAD_SAMPLING_DURATION_MAX_MS
    5000
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_THREAD_SAMPLING_DURATION_MAX_MS on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_THREAD_IDLE_CPU_PERCENT_THRESHOLD_MIN
    1
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_THREAD_IDLE_CPU_PERCENT_THRESHOLD_MIN on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_THREAD_IDLE_CPU_PERCENT_THRESHOLD_DEFAULT
    1
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_THREAD_IDLE_CPU_PERCENT_THRESHOLD_DEFAULT on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_THREAD_IDLE_CPU_PERCENT_THRESHOLD_MAX
    50
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_THREAD_IDLE_CPU_PERCENT_THRESHOLD_MAX on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_THREAD_STACK_HIGH_WATER_MARK_THRESHOLD_MIN_BYTES
    64
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_THREAD_STACK_HIGH_WATER_MARK_THRESHOLD_MIN_BYTES on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_THREAD_STACK_HIGH_WATER_MARK_THRESHOLD_DEFAULT_BYTES
    128
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_THREAD_STACK_HIGH_WATER_MARK_THRESHOLD_DEFAULT_BYTES on PC"
)
app_settings_add_pc_debug_config(
    DEBUG_THREAD_STACK_HIGH_WATER_MARK_THRESHOLD_MAX_BYTES
    8192
    "Default value of CONFIG_BROOKESIA_APP_SETTINGS_DEBUG_THREAD_STACK_HIGH_WATER_MARK_THRESHOLD_MAX_BYTES on PC"
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
brookesia_define_component_version(${COMPONENT_LIB} ${COMPONENT_DIR} BROOKESIA_APP_SETTINGS)

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
if(BROOKESIA_APP_SETTINGS_PC_CONFIG_ENABLE_PRELOAD_DOM)
    set(BROOKESIA_APP_SETTINGS_PC_PRELOAD_DOM_DEF CONFIG_BROOKESIA_APP_SETTINGS_ENABLE_PRELOAD_DOM=1)
else()
    set(BROOKESIA_APP_SETTINGS_PC_PRELOAD_DOM_DEF CONFIG_BROOKESIA_APP_SETTINGS_ENABLE_PRELOAD_DOM=0)
endif()

target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        BROOKESIA_APP_SETTINGS_PACKAGE_DIR="${COMPONENT_DIR}/package"
        "BROOKESIA_APP_SETTINGS_WIFI_MAX_VISIBLE_APS=(${BROOKESIA_APP_SETTINGS_PC_CONFIG_WIFI_LIST_PAGE_SIZE})"
        ${BROOKESIA_APP_SETTINGS_PC_MEMORY_TRACE_DEF}
        ${BROOKESIA_APP_SETTINGS_PC_PRELOAD_DOM_DEF}
        ${app_settings_pc_debug_config_compile_definitions}
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
