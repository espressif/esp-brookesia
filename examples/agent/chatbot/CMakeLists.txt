# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly

# Force Ninja to use response files to completely solve the 32KB Windows command line length limit
if("${CMAKE_GENERATOR}" MATCHES "Ninja")
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")
endif()

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Work around esp-idf-configdep mapping CONFIG_ESP_LVGL_ADAPTER_PARTIAL_AUX_IMG_CACHE
# to a Windows-invalid path containing the reserved device name "aux".
set(CONFIGDEP_ENABLE 0 CACHE BOOL "Disable esp-idf-configdep for Windows AUX path compatibility" FORCE)

project(example_agent_chatbot)

# Temporary workaround for `espressif__mcp-c-sdk` stringop-truncation warning
idf_build_set_property(COMPILE_OPTIONS -Wno-error=stringop-truncation APPEND)
