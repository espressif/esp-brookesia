set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_INCLUDE_DIRS ${COMPONENT_DIR}/include)

if(ESP_PLATFORM)
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/esp_platform.cmake)
else()
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/pc_platform.cmake)
endif()
