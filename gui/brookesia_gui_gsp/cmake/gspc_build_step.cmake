# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Build-time GSPC launcher used by brookesia_gsp_add_bundle().
#
# cmake -DESP_GSP_ROOT=<dir> -DPROJECT_DIR=<dir> [-DPYTHON=<python>]
#       [-DGSPC_EXECUTABLE=<gspc>] [-DCONFIG_ESP_GSP_ENABLE_JPEG=0|1]
#       -P gspc_build_step.cmake -- <gspc arguments>...

cmake_minimum_required(VERSION 3.22)

if(NOT ESP_GSP_ROOT OR NOT EXISTS "${ESP_GSP_ROOT}/cmake/gsp_toolchain.cmake")
    message(FATAL_ERROR "brookesia_gui_gsp: ESP_GSP_ROOT does not point to an esp-gsp component")
endif()

set(gspc_args "")
set(collect_args OFF)
math(EXPR last_arg "${CMAKE_ARGC} - 1")
foreach(arg_index RANGE ${last_arg})
    if(collect_args)
        list(APPEND gspc_args "${CMAKE_ARGV${arg_index}}")
    elseif("${CMAKE_ARGV${arg_index}}" STREQUAL "--")
        set(collect_args ON)
    endif()
endforeach()
if(NOT gspc_args)
    message(FATAL_ERROR "brookesia_gui_gsp: no GSPC arguments were passed to the build step")
endif()

set(menu_hint
    "To build without GSP, run 'idf.py menuconfig' and select "
    "ESP-Brookesia: System Core Configurations -> GUI backend -> LVGL.")
string(JOIN "" menu_hint ${menu_hint})

set(explicit_gspc "")
if(DEFINED GSPC_EXECUTABLE AND NOT "${GSPC_EXECUTABLE}" STREQUAL "")
    set(explicit_gspc "${GSPC_EXECUTABLE}")
elseif(DEFINED ENV{GSPC_EXECUTABLE} AND NOT "$ENV{GSPC_EXECUTABLE}" STREQUAL "")
    set(explicit_gspc "$ENV{GSPC_EXECUTABLE}")
endif()

if(explicit_gspc AND NOT EXISTS "${explicit_gspc}")
    message(FATAL_ERROR
        "brookesia_gui_gsp: GSP backend is selected, but GSPC_EXECUTABLE points to a missing file: "
        "${explicit_gspc}. ${menu_hint}")
endif()

include("${ESP_GSP_ROOT}/cmake/gsp_toolchain.cmake")
include("${ESP_GSP_ROOT}/cmake/gsp_compatibility.cmake")

esp_gsp_resolve_gspc("${ESP_GSP_ROOT}" gspc_command gspc_dependencies gspc_source)
esp_gsp_check_gspc_command(gspc_version ${gspc_command})
message(STATUS "esp-gsp: using ${gspc_version} from ${gspc_source}")

execute_process(
    COMMAND ${gspc_command} ${gspc_args}
    RESULT_VARIABLE gspc_result
)
if(NOT gspc_result EQUAL 0)
    message(FATAL_ERROR "brookesia_gui_gsp: GSPC failed with result ${gspc_result}")
endif()
