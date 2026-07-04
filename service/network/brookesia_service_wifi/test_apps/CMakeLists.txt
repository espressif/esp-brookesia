cmake_minimum_required(VERSION 3.16)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

set(TEST_APP_DIR ${CMAKE_CURRENT_LIST_DIR})
set(TEST_APP_MAIN_DIR ${TEST_APP_DIR}/main)

if(DEFINED ENV{IDF_PATH} AND NOT BROOKESIA_SERVICE_WIFI_TEST_APPS_FORCE_PC)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(test_brookesia_service_wifi)
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/esp_platform.cmake)
else()
    project(test_brookesia_service_wifi LANGUAGES CXX)
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/pc_platform.cmake)
endif()
