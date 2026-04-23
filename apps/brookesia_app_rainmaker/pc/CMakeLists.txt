# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# PC simulator: SDL2 + LVGL 8 + RainMaker (HAL → libcurl User API, same rainmaker_api_handle.c as firmware).
#
# Requires: SDL2, libcurl, ESP-IDF cJSON, rmaker_user_api headers (common_components or managed copy).
# From this directory:
#   export IDF_PATH=/path/to/esp-idf
#   cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
#   cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DRM_PC_SANITIZE_ADDRESS=ON   # optional: AddressSanitizer (Clang/GCC)
#   cmake --build build
#   lldb -- ./build/rainmaker_ui_pc   # on crash: (lldb) run … then bt all
#   ./build/rainmaker_ui_pc
#   ./build/rainmaker_ui_pc --width 1280 --height 720   # or -w 1280 -H 720; env RM_PC_LCD_HOR/VER
#   ./build/rainmaker_ui_pc_smoke && ctest --test-dir build   # pc_ui_smoke, pc_ui_smoke_320, pc_ui_smoke_1024
#   ./build/rainmaker_ui_layout_test && ctest -R pc_ui_layout   # geometry checks Select Devices row
#
# Requires: CMake 3.16+

cmake_minimum_required(VERSION 3.16)
project(rainmaker_pc_ui LANGUAGES C)

set(CMAKE_C_STANDARD 99)

option(RM_PC_SANITIZE_ADDRESS "Compile/link PC targets with -fsanitize=address (use with Debug; Clang or GCC)" OFF)
if(RM_PC_SANITIZE_ADDRESS)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address)
endif()

# LVGL: same tree as ESP-IDF Component Manager (idf.py → <product>/managed_components/lvgl__lvgl).
# Order: -DLVGL_ROOT, $ENV{LVGL_ROOT}, then common paths (phone_s3_box_3 product, repo-root managed_components, any products/*/managed_components).
if(NOT LVGL_ROOT)
  if(DEFINED ENV{LVGL_ROOT} AND NOT "$ENV{LVGL_ROOT}" STREQUAL "")
    set(LVGL_ROOT "$ENV{LVGL_ROOT}" CACHE PATH "LVGL root (lvgl__lvgl)")
  endif()
endif()
if(NOT LVGL_ROOT OR NOT EXISTS "${LVGL_ROOT}/lvgl.h")
  set(_lvgl_candidates
    "${CMAKE_CURRENT_LIST_DIR}/../../../products/phone/phone_s3_box_3/managed_components/lvgl__lvgl"
    "${CMAKE_CURRENT_LIST_DIR}/../../../../managed_components/lvgl__lvgl"
  )
  file(GLOB _lvgl_product_globs "${CMAKE_CURRENT_LIST_DIR}/../../../products/*/managed_components/lvgl__lvgl")
  if(_lvgl_product_globs)
    list(APPEND _lvgl_candidates ${_lvgl_product_globs})
  endif()
  foreach(_lvgl_c IN LISTS _lvgl_candidates)
    if(EXISTS "${_lvgl_c}/lvgl.h")
      set(LVGL_ROOT "${_lvgl_c}" CACHE PATH "LVGL root (lvgl__lvgl)" FORCE)
      break()
    endif()
  endforeach()
endif()

if(NOT EXISTS "${LVGL_ROOT}/lvgl.h")
  message(FATAL_ERROR
    "LVGL not found at LVGL_ROOT=${LVGL_ROOT}. "
    "Run idf.py in products/phone/phone_s3_box_3 to fetch managed_components, or set -DLVGL_ROOT=... / export LVGL_ROOT=...")
endif()

file(GLOB_RECURSE LVGL_SOURCES "${LVGL_ROOT}/src/*.c")

find_package(SDL2 REQUIRED CONFIG)
find_package(CURL)

if(DEFINED ENV{IDF_PATH})
  set(CJSON_SRC "$ENV{IDF_PATH}/components/json/cJSON/cJSON.c" CACHE FILEPATH "ESP-IDF cJSON.c")
endif()
if(NOT EXISTS "${CJSON_SRC}")
  message(FATAL_ERROR
    "PC full Home requires ESP-IDF cJSON. Set environment IDF_PATH to your esp-idf tree "
    "(e.g. export IDF_PATH=~/esp/esp-idf/components/.. parent) or set -DCJSON_SRC=/path/to/cJSON.c")
endif()
get_filename_component(CJSON_SRC_DIR "${CJSON_SRC}" DIRECTORY)

# app_rmaker_user_api.h: same headers as ESP-IDF Component Manager (espressif/rmaker_user_api).
# Default managed_components: prefer products/phone/phone_s3_box_3 (same as LVGL), then repo-root managed_components.
set(_RM_MANAGED_ROOT "")
foreach(_rm_cand
  "${CMAKE_CURRENT_LIST_DIR}/../../../products/phone/phone_s3_box_3/managed_components"
  "${CMAKE_CURRENT_LIST_DIR}/../../../../managed_components"
)
  if(EXISTS "${_rm_cand}")
    set(_RM_MANAGED_ROOT "${_rm_cand}")
    break()
  endif()
endforeach()
if(NOT _RM_MANAGED_ROOT)
  set(_RM_MANAGED_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../managed_components")
endif()
set(RMAKER_USER_API_INC "${_RM_MANAGED_ROOT}/espressif__rmaker_user_api/include" CACHE PATH "Directory containing app_rmaker_user_api.h")

if(NOT EXISTS "${RMAKER_USER_API_INC}/app_rmaker_user_api.h")
  file(GLOB _RM_API_HDR "${CMAKE_CURRENT_LIST_DIR}/../../../products/*/managed_components/*/include/app_rmaker_user_api.h")
  if(NOT _RM_API_HDR)
    file(GLOB _RM_API_HDR "${_RM_MANAGED_ROOT}/*/include/app_rmaker_user_api.h")
  endif()
  if(_RM_API_HDR)
    list(GET _RM_API_HDR 0 _RM_ONE)
    get_filename_component(RMAKER_USER_API_INC "${_RM_ONE}" DIRECTORY)
    set(RMAKER_USER_API_INC "${RMAKER_USER_API_INC}" CACHE PATH "rmaker_user_api include dir" FORCE)
  endif()
endif()

if(NOT EXISTS "${RMAKER_USER_API_INC}/app_rmaker_user_api.h")
  message(FATAL_ERROR
    "app_rmaker_user_api.h not found. Expected under ${_RM_MANAGED_ROOT}/espressif__rmaker_user_api/include "
    "(run: cd <esp_brookesia_phone> && idf.py reconfigure). Or set -DRMAKER_USER_API_INC=/path/to/include")
endif()

add_library(lvgl_pc STATIC ${LVGL_SOURCES})
target_include_directories(lvgl_pc PUBLIC "${LVGL_ROOT}" "${CMAKE_CURRENT_LIST_DIR}")
target_compile_definitions(
  lvgl_pc
  PUBLIC
    LV_CONF_INCLUDE_SIMPLE=1
    LV_LVGL_H_INCLUDE_SIMPLE=1
)
target_compile_options(lvgl_pc PRIVATE -Wall)

set(RM_UI "${CMAKE_CURRENT_LIST_DIR}/..")
set(RM_BACKEND "${RM_UI}/backend")

# Shared sources for SDL app and headless smoke test (same UI + backend objects).
set(RM_PC_COMMON_SOURCES
  rainmaker_backend_pc_stub.c
  app_rmaker_user_api_pc.c
  app_rmaker_user_transport_pc.c
  rainmaker_user_api_hal_pc.c
  "${RM_BACKEND}/rainmaker_api_handle.c"
  "${RM_BACKEND}/rainmaker_app_backend_shared.c"
  "${RM_BACKEND}/rainmaker_device_group_manager.c"
  "${RM_BACKEND}/rainmaker_list_manager.c"
  esp_random_pc.c
  rainmaker_standard_device_pc.c
  "${RM_BACKEND}/standard_device/rainmaker_standard_device_shared.c"
  "${RM_BACKEND}/standard_device/rainmaker_standard_light.c"
  "${RM_BACKEND}/standard_device/rainmaker_standard_switch.c"
  "${RM_BACKEND}/standard_device/rainmaker_standard_fan.c"
  rainmaker_app_backend_util_pc.c
  ui_pc_stubs.c
  "${RM_UI}/ui/ui_display.c"
  "${RM_UI}/ui/ui_common.c"
  "${RM_UI}/ui/ui_i18n.c"
  "${RM_UI}/ui/components/ui_comp_hook.c"
  "${RM_UI}/ui/screens/ui_Screen_Login.c"
  "${RM_UI}/ui/screens/ui_Screen_Home.c"
  "${RM_UI}/ui/screens/ui_Screen_Schedules.c"
  "${RM_UI}/ui/screens/ui_Screen_CreateSchedule.c"
  "${RM_UI}/ui/screens/ui_Screen_CreateScene.c"
  "${RM_UI}/ui/screens/ui_Screen_SelectDevices.c"
  "${RM_UI}/ui/screens/ui_Screen_SelectActions.c"
  "${RM_UI}/ui/screens/ui_Screen_Scenes.c"
  "${RM_UI}/ui/screens/ui_Screen_User.c"
  "${RM_UI}/ui/screens/ui_Screen_Light_Detail.c"
  "${RM_UI}/ui/screens/ui_Screen_Switch_Details.c"
  "${RM_UI}/ui/screens/ui_Screen_Fan_Details.c"
  "${RM_UI}/ui/screens/ui_Screen_Device_Setting.c"
  "${RM_UI}/ui/ui_gesture_nav.c"
  "${RM_UI}/ui/images/ui_img_logo_png.c"
  "${RM_UI}/ui/images/ui_img_home_png.c"
  "${RM_UI}/ui/images/devices/light_3_power_off.c"
  "${RM_UI}/ui/images/devices/light_3_power_on.c"
  "${RM_UI}/ui/images/devices/switch_power_off.c"
  "${RM_UI}/ui/images/devices/switch_power_on.c"
  "${RM_UI}/ui/images/devices/fan_power_off.c"
  "${RM_UI}/ui/images/devices/fan_power_on.c"
  "${RM_UI}/ui/fonts/lv_font_ui_zh_16.c"
  "${CJSON_SRC}"
)

add_executable(rainmaker_ui_pc main_pc.c lv_sdl_port.c ${RM_PC_COMMON_SOURCES})
add_executable(rainmaker_ui_pc_smoke ui_pc_smoke_test.c ${RM_PC_COMMON_SOURCES})
add_executable(rainmaker_ui_layout_test ui_pc_layout_test.c rainmaker_app_backend_layout_test.c ${RM_PC_COMMON_SOURCES})
add_executable(rainmaker_ui_visual_test ui_pc_visual_test.c rainmaker_app_backend_layout_test.c ${RM_PC_COMMON_SOURCES})

set(RM_PC_PRIVATE_INCLUDES
  "${CMAKE_CURRENT_LIST_DIR}"
  "${CMAKE_CURRENT_LIST_DIR}/include"
  "${CMAKE_CURRENT_LIST_DIR}/private_include"
  "${RMAKER_USER_API_INC}"
  "${RM_UI}/ui"
  "${RM_BACKEND}/include"
  "${RM_BACKEND}/private_inc"
  "${CJSON_SRC_DIR}"
)

foreach(_rm_pc_t rainmaker_ui_pc rainmaker_ui_pc_smoke rainmaker_ui_layout_test rainmaker_ui_visual_test)
  target_include_directories(${_rm_pc_t} PRIVATE ${RM_PC_PRIVATE_INCLUDES})
  target_compile_definitions(
    ${_rm_pc_t}
    PRIVATE
      RM_HOST_BUILD=1
      _POSIX_C_SOURCE=200809L
  )
  target_link_libraries(${_rm_pc_t} PRIVATE lvgl_pc pthread)
  if(CURL_FOUND)
    target_compile_definitions(${_rm_pc_t} PRIVATE RM_PC_HAVE_CURL=1)
    target_link_libraries(${_rm_pc_t} PRIVATE CURL::libcurl)
  endif()
endforeach()

target_link_libraries(rainmaker_ui_pc PRIVATE SDL2::SDL2)

enable_testing()
add_test(NAME pc_ui_smoke COMMAND rainmaker_ui_pc_smoke)
# Multi-resolution smoke (same binary; resolution via RM_PC_LCD_HOR / RM_PC_LCD_VER or -w/-H)
add_test(NAME pc_ui_smoke_320 COMMAND rainmaker_ui_pc_smoke)
set_tests_properties(
  pc_ui_smoke_320
  PROPERTIES
    ENVIRONMENT "RM_PC_LCD_HOR=320;RM_PC_LCD_VER=240"
)
add_test(NAME pc_ui_smoke_1024 COMMAND rainmaker_ui_pc_smoke)
set_tests_properties(
  pc_ui_smoke_1024
  PROPERTIES
    ENVIRONMENT "RM_PC_LCD_HOR=1024;RM_PC_LCD_VER=600"
)

add_test(NAME pc_ui_layout_default COMMAND rainmaker_ui_layout_test)
add_test(NAME pc_ui_layout_320 COMMAND rainmaker_ui_layout_test)
set_tests_properties(
  pc_ui_layout_320
  PROPERTIES
    ENVIRONMENT "RM_PC_LCD_HOR=320;RM_PC_LCD_VER=240"
)
add_test(NAME pc_ui_layout_1024 COMMAND rainmaker_ui_layout_test)
set_tests_properties(
  pc_ui_layout_1024
  PROPERTIES
    ENVIRONMENT "RM_PC_LCD_HOR=1024;RM_PC_LCD_VER=600"
)

# Visual regression tests
add_test(NAME pc_ui_visual_default COMMAND rainmaker_ui_visual_test -o visual_test_output)
add_test(NAME pc_ui_visual_320 COMMAND rainmaker_ui_visual_test -w 320 -H 240 -o visual_test_output)
add_test(NAME pc_ui_visual_480 COMMAND rainmaker_ui_visual_test -w 480 -H 272 -o visual_test_output)
add_test(NAME pc_ui_visual_800 COMMAND rainmaker_ui_visual_test -w 800 -H 480 -o visual_test_output)
add_test(NAME pc_ui_visual_1024 COMMAND rainmaker_ui_visual_test -w 1024 -H 600 -o visual_test_output)

if(CURL_FOUND)
  add_executable(http_smoke http_smoke.c)
  target_link_libraries(http_smoke PRIVATE CURL::libcurl)
  target_compile_definitions(http_smoke PRIVATE _POSIX_C_SOURCE=200809L)
endif()
