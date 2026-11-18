cmake_minimum_required(VERSION 3.16)

project(gsp_session_host_test LANGUAGES CXX)
find_package(Boost REQUIRED)
find_library(BOOST_THREAD_LIBRARY NAMES boost_thread REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GSP_COMPONENT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../..)

add_executable(test_session
    test_session.cpp
    ${GSP_COMPONENT_DIR}/src/app_host.cpp
    ${GSP_COMPONENT_DIR}/src/session.cpp
    ${GSP_COMPONENT_DIR}/src/bundle.cpp
    ${GSP_COMPONENT_DIR}/src/bundle_contract.cpp
)

target_compile_definitions(test_session PRIVATE
    BROOKESIA_GUI_GSP_HOST_TEST
    CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES=1
)
target_include_directories(test_session PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/stubs
    ${GSP_COMPONENT_DIR}/include
    ${GSP_COMPONENT_DIR}/src
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(test_session PRIVATE z ${BOOST_THREAD_LIBRARY} Threads::Threads)

add_library(fail_thread_create SHARED fail_thread_create.cpp)
target_link_libraries(fail_thread_create PRIVATE ${CMAKE_DL_LIBS})

enable_testing()
add_test(NAME gsp_session_lifecycle COMMAND test_session)
add_test(NAME gsp_session_cleanup_task_failure COMMAND test_session)
set_tests_properties(gsp_session_cleanup_task_failure PROPERTIES
    ENVIRONMENT "LD_PRELOAD=$<TARGET_FILE:fail_thread_create>;GSP_TEST_FAIL_CLEANUP_THREAD=1"
)
