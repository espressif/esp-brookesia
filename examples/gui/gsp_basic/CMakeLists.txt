# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly

if("${CMAKE_GENERATOR}" MATCHES "Ninja")
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")
endif()

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(example_gui_gsp_basic)

brookesia_compile_tuning_apply_current_project()
