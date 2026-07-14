#
# PC Platform
#
set(COMPONENT_LIB brookesia_app_store_impl)

set(
    BROOKESIA_APP_STORE_PC_CONFIG_SERVER_ROOT_URL
    "https://brookesia-app-store.espressif.com/api/v1"
    CACHE STRING
    "Default App Store API root URL for PC simulator"
)
set(
    BROOKESIA_APP_STORE_PC_CONFIG_INDEX_URL
    ""
    CACHE STRING
    "Optional legacy App Store index URL override for PC simulator"
)
set(
    BROOKESIA_APP_STORE_PC_CONFIG_LIST_PAGE_SIZE
    5
    CACHE STRING
    "Number of App Store rows shown per page on PC"
)
option(
    BROOKESIA_APP_STORE_PC_CONFIG_ENABLE_PRELOAD_DOM
    "Default value of CONFIG_BROOKESIA_APP_STORE_ENABLE_PRELOAD_DOM on PC"
    ON
)

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)
brookesia_define_component_version(${COMPONENT_LIB} ${COMPONENT_DIR} BROOKESIA_APP_STORE)

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
        brookesia::service_helper
        brookesia::service_http
        brookesia::service_device
        brookesia::service_storage
        "-u app_store_provider_symbol"
)
if(BROOKESIA_APP_STORE_PC_CONFIG_ENABLE_PRELOAD_DOM)
    set(BROOKESIA_APP_STORE_PC_PRELOAD_DOM_DEF CONFIG_BROOKESIA_APP_STORE_ENABLE_PRELOAD_DOM=1)
else()
    set(BROOKESIA_APP_STORE_PC_PRELOAD_DOM_DEF CONFIG_BROOKESIA_APP_STORE_ENABLE_PRELOAD_DOM=0)
endif()

target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        BROOKESIA_APP_STORE_PACKAGE_DIR="${COMPONENT_DIR}/package"
        BROOKESIA_APP_STORE_SERVER_ROOT_URL="${BROOKESIA_APP_STORE_PC_CONFIG_SERVER_ROOT_URL}"
        BROOKESIA_APP_STORE_INDEX_URL="${BROOKESIA_APP_STORE_PC_CONFIG_INDEX_URL}"
        BROOKESIA_APP_STORE_LIST_PAGE_SIZE=${BROOKESIA_APP_STORE_PC_CONFIG_LIST_PAGE_SIZE}
        ${BROOKESIA_APP_STORE_PC_PRELOAD_DOM_DEF}
)

set(app_store_package_id "brookesia.general.app_store")
include(${BROOKESIA_SYSTEM_CORE_COMPONENT_DIR}/cmake/runtime_app_stage.cmake)
if(NOT DEFINED BROOKESIA_SYSTEM_CORE_PC_STAGE_APP_ROOT)
    set(BROOKESIA_SYSTEM_CORE_PC_STAGE_APP_ROOT "${CMAKE_BINARY_DIR}/brookesia/apps")
endif()
set(app_store_stage_root "${BROOKESIA_SYSTEM_CORE_PC_STAGE_APP_ROOT}")
brookesia_stage_runtime_app_package(
    PACKAGE_ID "${app_store_package_id}"
    SOURCE_DIR "${COMPONENT_DIR}/package"
    STAGE_ROOT "${app_store_stage_root}"
    NO_INDEX
)

if(DEFINED BROOKESIA_APP_STORE_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_APP_STORE_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_APP_STORE_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::app_store ALIAS ${COMPONENT_LIB})
