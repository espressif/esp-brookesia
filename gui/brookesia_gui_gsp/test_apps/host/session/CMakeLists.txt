cmake_minimum_required(VERSION 3.16)

project(gsp_session_host_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GSP_COMPONENT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../..)

add_executable(test_session
    test_session.cpp
    ${GSP_COMPONENT_DIR}/src/app_host.cpp
    ${GSP_COMPONENT_DIR}/src/session.cpp
    ${GSP_COMPONENT_DIR}/src/bundle.cpp
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
)
target_link_libraries(test_session PRIVATE z)
