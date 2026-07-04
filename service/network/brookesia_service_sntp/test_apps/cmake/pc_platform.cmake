include(/../../../system/brookesia_service_storage/cmake/pc_test_common.cmake)

brookesia_service_storage_init_pc_defaults()

set(
    BROOKESIA_TEST_BOOST_ROOT
    ""
    CACHE PATH "Optional Boost source root used when system Boost.Test is unavailable"
)
set(
    BROOKESIA_SERVICE_SNTP_TEST_APPS_PC_KV_DIR
    "${BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_KV_DIR}"
    CACHE PATH "Directory used by PC SNTP tests for HAL key-value data"
)
set(
    BROOKESIA_SERVICE_SNTP_TEST_APPS_PC_FS_ROOT
    "${BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_FS_ROOT}"
    CACHE PATH "Directory used by PC SNTP tests for file-system mounts"
)
set(
    BROOKESIA_SERVICE_SNTP_TEST_APPS_PC_ROOTFS_DIR
    "${BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_ROOTFS_DIR}"
    CACHE PATH "Reserved launcher rootfs directory used by PC SNTP tests"
)
set(
    BROOKESIA_SERVICE_SNTP_TEST_APPS_WIFI_SSID
    "pc_stub_ap_1"
    CACHE STRING "Wi-Fi SSID used by PC SNTP tests"
)
set(
    BROOKESIA_SERVICE_SNTP_TEST_APPS_WIFI_PASSWORD
    "password1"
    CACHE STRING "Wi-Fi password used by PC SNTP tests"
)
set(
    BROOKESIA_SERVICE_SNTP_TEST_APPS_NTP_SERVER
    "pool.ntp.org"
    CACHE STRING "NTP server used by PC SNTP network tests"
)
set(
    BROOKESIA_SERVICE_SNTP_TEST_APPS_TIMEZONE
    "UTC"
    CACHE STRING "Timezone used by PC SNTP tests"
)
set(
    BROOKESIA_SERVICE_SNTP_TEST_APPS_SYNC_TIMEOUT_MS
    "60000"
    CACHE STRING "Maximum time to wait for PC SNTP sync"
)
set(
    BROOKESIA_SERVICE_SNTP_TEST_APPS_MIN_VALID_UNIX_TIME
    "1704067200"
    CACHE STRING "Minimum valid Unix time accepted by PC SNTP tests"
)
set(
    BROOKESIA_SERVICE_SNTP_TEST_APPS_WIFI_BACKEND
    "stub"
    CACHE STRING "HAL Linux Wi-Fi backend used by PC SNTP tests"
)
set_property(CACHE BROOKESIA_SERVICE_SNTP_TEST_APPS_WIFI_BACKEND PROPERTY STRINGS auto stub networkmanager)
set(BROOKESIA_HAL_LINUX_WIFI_BACKEND "${BROOKESIA_SERVICE_SNTP_TEST_APPS_WIFI_BACKEND}" CACHE STRING "" FORCE)

if(NOT TARGET brookesia::service_sntp)
    add_subdirectory(${TEST_APP_DIR}/.. ${CMAKE_BINARY_DIR}/brookesia_service_sntp)
endif()
if(NOT TARGET brookesia::service_wifi)
    add_subdirectory(${TEST_APP_DIR}/../../brookesia_service_wifi ${CMAKE_BINARY_DIR}/brookesia_service_wifi)
endif()
if(NOT TARGET brookesia::hal_linux)
    add_subdirectory(
        ${TEST_APP_DIR}/../../../hal/brookesia_hal_linux
        ${CMAKE_BINARY_DIR}/brookesia_hal_linux
    )
endif()

file(GLOB TEST_APP_SRCS_CPP ${TEST_APP_MAIN_DIR}/*.cpp)
add_executable(test_brookesia_service_sntp ${TEST_APP_SRCS_CPP})

find_package(Boost COMPONENTS unit_test_framework QUIET)
if(Boost_unit_test_framework_FOUND)
    target_link_libraries(test_brookesia_service_sntp PRIVATE Boost::unit_test_framework)
    target_compile_definitions(test_brookesia_service_sntp PRIVATE BOOST_TEST_DYN_LINK)
else()
    if("${BROOKESIA_TEST_BOOST_ROOT}" STREQUAL "")
        message(FATAL_ERROR "Boost.Test not found. Set BROOKESIA_TEST_BOOST_ROOT to an esp-boost/src path.")
    endif()
    target_include_directories(test_brookesia_service_sntp PRIVATE ${BROOKESIA_TEST_BOOST_ROOT})
    target_compile_definitions(test_brookesia_service_sntp PRIVATE
        BROOKESIA_LIB_UTILS_TEST_ADAPTER_USE_INCLUDED_BOOST_TEST=1
    )
endif()

target_compile_features(test_brookesia_service_sntp PRIVATE cxx_std_23)
target_include_directories(test_brookesia_service_sntp PRIVATE ${TEST_APP_MAIN_DIR})
target_compile_definitions(test_brookesia_service_sntp PRIVATE
    TEST_SNTP_PC_KV_DIR="${BROOKESIA_SERVICE_SNTP_TEST_APPS_PC_KV_DIR}"
    TEST_SNTP_ENABLE_NETWORK=1
    TEST_SNTP_USE_WIFI=1
    TEST_SNTP_WIFI_SSID="${BROOKESIA_SERVICE_SNTP_TEST_APPS_WIFI_SSID}"
    TEST_SNTP_WIFI_PASSWORD="${BROOKESIA_SERVICE_SNTP_TEST_APPS_WIFI_PASSWORD}"
    TEST_SNTP_NTP_SERVER="${BROOKESIA_SERVICE_SNTP_TEST_APPS_NTP_SERVER}"
    TEST_SNTP_TIMEZONE="${BROOKESIA_SERVICE_SNTP_TEST_APPS_TIMEZONE}"
    "TEST_SNTP_SYNC_TIMEOUT_MS=(${BROOKESIA_SERVICE_SNTP_TEST_APPS_SYNC_TIMEOUT_MS})"
    "TEST_SNTP_MIN_VALID_UNIX_TIME=(${BROOKESIA_SERVICE_SNTP_TEST_APPS_MIN_VALID_UNIX_TIME})"
)
target_link_libraries(
    test_brookesia_service_sntp
    PRIVATE
        brookesia::service_sntp
        brookesia::service_storage
        brookesia::service_wifi
        brookesia::hal_linux
)

set(_sntp_test_launcher ${CMAKE_CURRENT_BINARY_DIR}/run_test_brookesia_service_sntp.sh)
brookesia_service_storage_generate_proot_launcher(
    OUTPUT ${_sntp_test_launcher}
    TARGET_NAME test_brookesia_service_sntp
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    KV_DIR ${BROOKESIA_SERVICE_SNTP_TEST_APPS_PC_KV_DIR}
    FS_ROOT ${BROOKESIA_SERVICE_SNTP_TEST_APPS_PC_FS_ROOT}
    ROOTFS_DIR ${BROOKESIA_SERVICE_SNTP_TEST_APPS_PC_ROOTFS_DIR}
)

enable_testing()
add_test(
    NAME test_brookesia_service_sntp
    COMMAND /usr/bin/env bash ${_sntp_test_launcher}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
