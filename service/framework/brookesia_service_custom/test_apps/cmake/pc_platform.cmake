set(
    BROOKESIA_TEST_BOOST_ROOT
    ""
    CACHE PATH "Optional Boost source root used when system Boost.Test is unavailable"
)

add_subdirectory(${TEST_APP_DIR}/.. ${CMAKE_BINARY_DIR}/brookesia_service_custom)

file(GLOB TEST_APP_SRCS_CPP ${TEST_APP_MAIN_DIR}/*.cpp)
add_executable(test_brookesia_service_custom ${TEST_APP_SRCS_CPP})

find_package(Boost COMPONENTS unit_test_framework QUIET)
if(Boost_unit_test_framework_FOUND)
    target_link_libraries(test_brookesia_service_custom PRIVATE Boost::unit_test_framework)
    target_compile_definitions(test_brookesia_service_custom PRIVATE BOOST_TEST_DYN_LINK)
else()
    if("${BROOKESIA_TEST_BOOST_ROOT}" STREQUAL "")
        message(FATAL_ERROR "Boost.Test not found. Set BROOKESIA_TEST_BOOST_ROOT to an esp-boost/src path.")
    endif()
    target_include_directories(test_brookesia_service_custom PRIVATE ${BROOKESIA_TEST_BOOST_ROOT})
    target_compile_definitions(test_brookesia_service_custom PRIVATE
        BROOKESIA_LIB_UTILS_TEST_ADAPTER_USE_INCLUDED_BOOST_TEST=1
    )
endif()

target_compile_features(test_brookesia_service_custom PRIVATE cxx_std_23)
target_include_directories(test_brookesia_service_custom PRIVATE ${TEST_APP_MAIN_DIR})
target_link_libraries(
    test_brookesia_service_custom
    PRIVATE
        brookesia::service_custom
)

enable_testing()
add_test(NAME test_brookesia_service_custom COMMAND test_brookesia_service_custom)
