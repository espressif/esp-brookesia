option(
    BROOKESIA_SERVICE_HTTP_TEST_APPS_ENABLE_NETWORK
    "Enable PC HTTP service tests against an external endpoint"
    ON
)
set(
    BROOKESIA_SERVICE_HTTP_TEST_APPS_ECHO_URL
    "https://echo.apifox.com"
    CACHE STRING "Base URL used by PC HTTP service tests"
)
set(
    BROOKESIA_TEST_BOOST_ROOT
    ""
    CACHE PATH "Optional Boost source root used when system Boost.Test is unavailable"
)

list(APPEND BROOKESIA_SERVICE_HTTP_PC_COMPILE_DEFINITIONS
    BROOKESIA_SERVICE_HTTP_ENABLE_HTTP=1
    BROOKESIA_SERVICE_HTTP_ENABLE_HTTPS=1
    BROOKESIA_SERVICE_HTTP_REQUIRE_TIME_SYNC=0
)

add_subdirectory(${TEST_APP_DIR}/.. ${CMAKE_BINARY_DIR}/brookesia_service_http)

if(NOT TARGET brookesia::hal_linux)
    add_subdirectory(
        ${TEST_APP_DIR}/../../../hal/brookesia_hal_linux
        ${CMAKE_BINARY_DIR}/brookesia_hal_linux
    )
endif()

file(GLOB TEST_APP_SRCS_CPP ${TEST_APP_MAIN_DIR}/*.cpp)
add_executable(test_brookesia_service_http ${TEST_APP_SRCS_CPP})

find_package(Boost COMPONENTS unit_test_framework QUIET)
if(Boost_unit_test_framework_FOUND)
    target_link_libraries(test_brookesia_service_http PRIVATE Boost::unit_test_framework)
    target_compile_definitions(test_brookesia_service_http PRIVATE BOOST_TEST_DYN_LINK)
else()
    if("${BROOKESIA_TEST_BOOST_ROOT}" STREQUAL "")
        message(FATAL_ERROR "Boost.Test not found. Set BROOKESIA_TEST_BOOST_ROOT to an esp-boost/src path.")
    endif()
    target_include_directories(test_brookesia_service_http PRIVATE ${BROOKESIA_TEST_BOOST_ROOT})
    target_compile_definitions(test_brookesia_service_http PRIVATE
        BROOKESIA_LIB_UTILS_TEST_ADAPTER_USE_INCLUDED_BOOST_TEST=1
    )
endif()

target_compile_features(test_brookesia_service_http PRIVATE cxx_std_23)
target_include_directories(test_brookesia_service_http PRIVATE ${TEST_APP_MAIN_DIR})
target_compile_definitions(test_brookesia_service_http PRIVATE
    TEST_HTTP_ENABLE_NETWORK=$<BOOL:${BROOKESIA_SERVICE_HTTP_TEST_APPS_ENABLE_NETWORK}>
    TEST_HTTP_USE_WIFI=0
    TEST_HTTP_ECHO_URL="${BROOKESIA_SERVICE_HTTP_TEST_APPS_ECHO_URL}"
)
target_link_libraries(test_brookesia_service_http PRIVATE brookesia::service_http brookesia::hal_linux)

enable_testing()
add_test(
    NAME test_brookesia_service_http
    COMMAND test_brookesia_service_http
)
