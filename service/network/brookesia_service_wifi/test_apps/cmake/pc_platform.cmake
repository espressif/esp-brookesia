include(/../../../system/brookesia_service_storage/cmake/pc_test_common.cmake)

brookesia_service_storage_init_pc_defaults()

set(
    BROOKESIA_TEST_BOOST_ROOT
    ""
    CACHE PATH "Optional Boost source root used when system Boost.Test is unavailable"
)
set(
    BROOKESIA_SERVICE_WIFI_TEST_APPS_PC_KV_DIR
    "${BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_KV_DIR}"
    CACHE PATH "Directory used by PC Wi-Fi tests for HAL key-value data"
)
set(
    BROOKESIA_SERVICE_WIFI_TEST_APPS_PC_FS_ROOT
    "${BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_FS_ROOT}"
    CACHE PATH "Directory used by PC Wi-Fi tests for file-system mounts"
)
set(
    BROOKESIA_SERVICE_WIFI_TEST_APPS_PC_ROOTFS_DIR
    "${BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_ROOTFS_DIR}"
    CACHE PATH "Reserved launcher rootfs directory used by PC Wi-Fi tests"
)
set(
    BROOKESIA_HAL_LINUX_WIFI_BACKEND
    "stub"
    CACHE STRING "HAL Linux Wi-Fi backend used by PC Wi-Fi service tests"
)

if(NOT TARGET brookesia::service_storage)
    add_subdirectory(
        /../../../system/brookesia_service_storage
        ${CMAKE_BINARY_DIR}/brookesia_service_storage
    )
endif()
if(NOT TARGET brookesia::service_wifi)
    add_subdirectory(${TEST_APP_DIR}/.. ${CMAKE_BINARY_DIR}/brookesia_service_wifi)
endif()
if(NOT TARGET brookesia::hal_linux)
    add_subdirectory(
        ${TEST_APP_DIR}/../../../hal/brookesia_hal_linux
        ${CMAKE_BINARY_DIR}/brookesia_hal_linux
    )
endif()

file(GLOB TEST_APP_SRCS_CPP ${TEST_APP_MAIN_DIR}/*.cpp)
add_executable(test_brookesia_service_wifi ${TEST_APP_SRCS_CPP})

find_package(Boost COMPONENTS unit_test_framework QUIET)
if(Boost_unit_test_framework_FOUND)
    target_link_libraries(test_brookesia_service_wifi PRIVATE Boost::unit_test_framework)
    target_compile_definitions(test_brookesia_service_wifi PRIVATE BOOST_TEST_DYN_LINK)
else()
    if("${BROOKESIA_TEST_BOOST_ROOT}" STREQUAL "")
        message(FATAL_ERROR "Boost.Test not found. Set BROOKESIA_TEST_BOOST_ROOT to an esp-boost/src path.")
    endif()
    target_include_directories(test_brookesia_service_wifi PRIVATE ${BROOKESIA_TEST_BOOST_ROOT})
    target_compile_definitions(test_brookesia_service_wifi PRIVATE
        BROOKESIA_LIB_UTILS_TEST_ADAPTER_USE_INCLUDED_BOOST_TEST=1
    )
endif()

target_compile_features(test_brookesia_service_wifi PRIVATE cxx_std_23)
target_include_directories(test_brookesia_service_wifi PRIVATE ${TEST_APP_MAIN_DIR})
target_compile_definitions(test_brookesia_service_wifi PRIVATE
    TEST_WIFI_PC_KV_DIR="${BROOKESIA_SERVICE_WIFI_TEST_APPS_PC_KV_DIR}"
    CI_TEST_WIFI_2_4G_AP1_SSID="pc_stub_ap_1"
    CI_TEST_WIFI_2_4G_AP1_PSW="password1"
    CI_TEST_WIFI_2_4G_AP2_SSID="pc_stub_ap_2"
    CI_TEST_WIFI_2_4G_AP2_PSW="password2"
)
target_link_libraries(
    test_brookesia_service_wifi
    PRIVATE
        brookesia::service_wifi
        brookesia::service_storage
        brookesia::hal_linux
)

set(_wifi_test_launcher ${CMAKE_CURRENT_BINARY_DIR}/run_test_brookesia_service_wifi.sh)
brookesia_service_storage_generate_proot_launcher(
    OUTPUT ${_wifi_test_launcher}
    TARGET_NAME test_brookesia_service_wifi
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    KV_DIR ${BROOKESIA_SERVICE_WIFI_TEST_APPS_PC_KV_DIR}
    FS_ROOT ${BROOKESIA_SERVICE_WIFI_TEST_APPS_PC_FS_ROOT}
    ROOTFS_DIR ${BROOKESIA_SERVICE_WIFI_TEST_APPS_PC_ROOTFS_DIR}
)

enable_testing()
add_test(
    NAME test_brookesia_service_wifi
    COMMAND /usr/bin/env bash ${_wifi_test_launcher}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
