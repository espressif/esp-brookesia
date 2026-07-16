if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

set(BROOKESIA_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

option(BROOKESIA_BUILD_LIB_UTILS "Build brookesia_lib_utils for PC" ON)
option(BROOKESIA_BUILD_HAL_INTERFACE "Build brookesia_hal_interface for PC" ON)
option(BROOKESIA_BUILD_HAL_WASM "Build brookesia_hal_wasm for PC" OFF)
option(BROOKESIA_BUILD_SERVICE_MANAGER "Build brookesia_service_manager for PC" ON)
option(BROOKESIA_BUILD_SERVICE_HELPER "Build brookesia_service_helper for PC" ON)
option(BROOKESIA_BUILD_SERVICE_STORAGE "Build brookesia_service_storage for PC" OFF)
option(BROOKESIA_BUILD_SERVICE_SNTP "Build brookesia_service_sntp for PC" OFF)
option(BROOKESIA_BUILD_SERVICE_HTTP "Build brookesia_service_http for PC" OFF)
option(BROOKESIA_BUILD_SERVICE_OTA "Build brookesia_service_ota for PC" OFF)
option(BROOKESIA_BUILD_SERVICE_DEVICE "Build brookesia_service_device for PC" OFF)
option(BROOKESIA_BUILD_SERVICE_AUDIO "Build brookesia_service_audio for PC" OFF)
option(BROOKESIA_BUILD_SERVICE_WIFI "Build brookesia_service_wifi for PC" OFF)
option(BROOKESIA_BUILD_SERVICE_DISPLAY "Build brookesia_service_display for PC" OFF)
option(BROOKESIA_BUILD_SERVICE_PICODET "Build brookesia_service_picodet for PC" OFF)
option(BROOKESIA_BUILD_SERVICE_VIDEO "Build brookesia_service_video for PC" OFF)
option(BROOKESIA_BUILD_EMULATION_NES "Build brookesia_emulation_nes for PC" OFF)
option(BROOKESIA_BUILD_GUI_INTERFACE "Build brookesia_gui_interface for PC" ON)
option(BROOKESIA_BUILD_GUI_LVGL "Build brookesia_gui_lvgl for PC" OFF)
option(BROOKESIA_BUILD_RUNTIME_MANAGER "Build brookesia_runtime_manager for PC" OFF)
option(BROOKESIA_BUILD_RUNTIME_LUA "Build brookesia_runtime_lua for PC" OFF)
option(BROOKESIA_BUILD_RUNTIME_JS "Build brookesia_runtime_js for PC" OFF)
option(BROOKESIA_BUILD_RUNTIME_WASM "Build brookesia_runtime_wasm for PC" OFF)
option(BROOKESIA_BUILD_RUNTIME_ELF "Build brookesia_runtime_elf for PC" OFF)
option(BROOKESIA_BUILD_SYSTEM_CORE "Build brookesia_system_core for PC" OFF)
option(BROOKESIA_BUILD_SYSTEM_SUPER "Build brookesia_system_super for PC" OFF)
option(BROOKESIA_BUILD_ALL_APPS "Build all app components for PC" OFF)

set(BROOKESIA_SUPPORTED_APP_COMPONENTS "")
if(NOT DEFINED BROOKESIA_EXCLUDE_APP_COMPONENTS)
    set(BROOKESIA_EXCLUDE_APP_COMPONENTS "")
endif()
file(GLOB BROOKESIA_APP_COMPONENT_DIRS CONFIGURE_DEPENDS LIST_DIRECTORIES true "${BROOKESIA_ROOT_DIR}/app/*")
foreach(app_component_dir IN LISTS BROOKESIA_APP_COMPONENT_DIRS)
    if(NOT IS_DIRECTORY "${app_component_dir}" OR NOT EXISTS "${app_component_dir}/CMakeLists.txt")
        continue()
    endif()
    get_filename_component(app_component "${app_component_dir}" NAME)
    if(NOT app_component MATCHES "^brookesia_app_")
        continue()
    endif()
    if(app_component IN_LIST BROOKESIA_EXCLUDE_APP_COMPONENTS)
        continue()
    endif()
    string(REGEX REPLACE "^brookesia_app_" "" app_name "${app_component}")
    string(TOUPPER "${app_name}" app_name_upper)
    string(REPLACE "-" "_" app_name_upper "${app_name_upper}")
    set(app_option "BROOKESIA_BUILD_APP_${app_name_upper}")
    option(${app_option} "Build ${app_component} for PC" ${BROOKESIA_BUILD_ALL_APPS})
    list(APPEND BROOKESIA_SUPPORTED_APP_COMPONENTS ${app_component})
endforeach()

set(BROOKESIA_SUPPORTED_COMPONENTS
    brookesia_lib_utils
    brookesia_hal_interface
    brookesia_hal_wasm
    brookesia_service_manager
    brookesia_service_helper
    brookesia_service_storage
    brookesia_service_sntp
    brookesia_service_http
    brookesia_service_ota
    brookesia_service_device
    brookesia_service_audio
    brookesia_service_wifi
    brookesia_service_display
    brookesia_service_picodet
    brookesia_service_video
    brookesia_emulation_nes
    brookesia_gui_interface
    brookesia_gui_lvgl
    brookesia_runtime_manager
    brookesia_runtime_lua
    brookesia_runtime_js
    brookesia_runtime_wasm
    brookesia_runtime_elf
    brookesia_system_core
    brookesia_system_super
)
list(APPEND BROOKESIA_SUPPORTED_COMPONENTS ${BROOKESIA_SUPPORTED_APP_COMPONENTS})

if(BROOKESIA_BUILD_LIB_UTILS)
    message(STATUS "Brookesia: enabling brookesia::lib_utils")
    add_subdirectory(utils/brookesia_lib_utils)
endif()

if(BROOKESIA_BUILD_HAL_INTERFACE)
    if(NOT TARGET brookesia::lib_utils)
        message(FATAL_ERROR "brookesia_hal_interface requires brookesia_lib_utils to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::hal_interface")
    add_subdirectory(hal/brookesia_hal_interface)
endif()

if(BROOKESIA_BUILD_HAL_WASM)
    if(NOT TARGET brookesia::lib_utils)
        message(FATAL_ERROR "brookesia_hal_wasm requires brookesia_lib_utils to be enabled")
    endif()
    if(NOT TARGET brookesia::hal_interface)
        message(FATAL_ERROR "brookesia_hal_wasm requires brookesia_hal_interface to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::hal_wasm")
    add_subdirectory(hal/brookesia_hal_wasm)
endif()

if(BROOKESIA_BUILD_SERVICE_MANAGER)
    if(NOT TARGET brookesia::lib_utils)
        message(FATAL_ERROR "brookesia_service_manager requires brookesia_lib_utils to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::service_manager")
    add_subdirectory(service/framework/brookesia_service_manager)
endif()

if(BROOKESIA_BUILD_SERVICE_HELPER)
    if(NOT TARGET brookesia::service_manager)
        message(FATAL_ERROR "brookesia_service_helper requires brookesia_service_manager to be enabled")
    endif()
    if(NOT TARGET brookesia::hal_interface)
        message(FATAL_ERROR "brookesia_service_helper requires brookesia_hal_interface to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::service_helper")
    add_subdirectory(service/framework/brookesia_service_helper)
endif()

if(BROOKESIA_BUILD_SERVICE_STORAGE)
    if(NOT TARGET brookesia::service_helper)
        message(FATAL_ERROR "brookesia_service_storage requires brookesia_service_helper to be enabled")
    endif()
    if(NOT TARGET brookesia::hal_interface)
        message(FATAL_ERROR "brookesia_service_storage requires brookesia_hal_interface to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::service_storage")
    add_subdirectory(service/system/brookesia_service_storage)
endif()

if(BROOKESIA_BUILD_SERVICE_WIFI)
    if(NOT TARGET brookesia::service_storage)
        message(FATAL_ERROR "brookesia_service_wifi requires brookesia_service_storage to be enabled")
    endif()
    if(NOT TARGET brookesia::service_helper)
        message(FATAL_ERROR "brookesia_service_wifi requires brookesia_service_helper to be enabled")
    endif()
    if(NOT TARGET brookesia::hal_interface)
        message(FATAL_ERROR "brookesia_service_wifi requires brookesia_hal_interface to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::service_wifi")
    add_subdirectory(service/network/brookesia_service_wifi)
endif()

if(BROOKESIA_BUILD_SERVICE_SNTP)
    if(NOT TARGET brookesia::service_storage)
        message(FATAL_ERROR "brookesia_service_sntp requires brookesia_service_storage to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::service_sntp")
    add_subdirectory(service/network/brookesia_service_sntp)
endif()

if(BROOKESIA_BUILD_SERVICE_HTTP)
    if(NOT TARGET brookesia::service_helper)
        message(FATAL_ERROR "brookesia_service_http requires brookesia_service_helper to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::service_http")
    add_subdirectory(service/network/brookesia_service_http)
endif()

if(BROOKESIA_BUILD_SERVICE_OTA)
    if(NOT TARGET brookesia::service_http)
        message(FATAL_ERROR "brookesia_service_ota requires brookesia_service_http to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::service_ota")
    add_subdirectory(service/system/brookesia_service_ota)
endif()

if(BROOKESIA_BUILD_SERVICE_DEVICE)
    if(NOT TARGET brookesia::service_helper)
        message(FATAL_ERROR "brookesia_service_device requires brookesia_service_helper to be enabled")
    endif()
    if(NOT TARGET brookesia::hal_interface)
        message(FATAL_ERROR "brookesia_service_device requires brookesia_hal_interface to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::service_device")
    add_subdirectory(service/system/brookesia_service_device)
endif()

if(BROOKESIA_BUILD_SERVICE_AUDIO)
    if(NOT TARGET brookesia::service_helper)
        message(FATAL_ERROR "brookesia_service_audio requires brookesia_service_helper to be enabled")
    endif()
    if(NOT TARGET brookesia::service_device)
        message(FATAL_ERROR "brookesia_service_audio requires brookesia_service_device to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::service_audio")
    add_subdirectory(service/media/brookesia_service_audio)
endif()

if(BROOKESIA_BUILD_SERVICE_DISPLAY)
    if(NOT TARGET brookesia::service_helper)
        message(FATAL_ERROR "brookesia_service_display requires brookesia_service_helper to be enabled")
    endif()
    if(NOT TARGET brookesia::hal_interface)
        message(FATAL_ERROR "brookesia_service_display requires brookesia_hal_interface to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::service_display")
    add_subdirectory(service/media/brookesia_service_display)
endif()

if(BROOKESIA_BUILD_SERVICE_PICODET)
    if(NOT TARGET brookesia::service_helper)
        message(FATAL_ERROR "brookesia_service_picodet requires brookesia_service_helper to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::service_picodet")
    add_subdirectory(service/media/brookesia_service_picodet)
endif()

if(BROOKESIA_BUILD_SERVICE_VIDEO)
    if(NOT TARGET brookesia::service_display)
        message(FATAL_ERROR "brookesia_service_video requires brookesia_service_display to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::service_video")
    add_subdirectory(service/media/brookesia_service_video)
endif()

if(BROOKESIA_BUILD_EMULATION_NES)
    if(NOT TARGET brookesia::service_display)
        message(FATAL_ERROR "brookesia_emulation_nes requires brookesia_service_display to be enabled")
    endif()
    if(NOT TARGET brookesia::service_audio)
        message(FATAL_ERROR "brookesia_emulation_nes requires brookesia_service_audio to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::emulation_nes")
    add_subdirectory(service/emulation/brookesia_emulation_nes)
endif()

if(BROOKESIA_BUILD_GUI_INTERFACE)
    if(NOT TARGET brookesia::lib_utils)
        message(FATAL_ERROR "brookesia_gui_interface requires brookesia_lib_utils to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::gui_interface")
    add_subdirectory(gui/brookesia_gui_interface)
endif()

if(BROOKESIA_BUILD_GUI_LVGL)
    if(NOT TARGET brookesia::gui_interface)
        message(FATAL_ERROR "brookesia_gui_lvgl requires brookesia_gui_interface to be enabled")
    endif()

    if(NOT TARGET lvgl)
        message(FATAL_ERROR "brookesia_gui_lvgl requires an existing lvgl target")
    endif()

    message(STATUS "Brookesia: enabling brookesia::gui_lvgl")
    add_subdirectory(gui/brookesia_gui_lvgl)
endif()

if(BROOKESIA_BUILD_RUNTIME_MANAGER)
    if(NOT TARGET brookesia::lib_utils)
        message(FATAL_ERROR "brookesia_runtime_manager requires brookesia_lib_utils to be enabled")
    endif()
    if(NOT TARGET brookesia::service_manager)
        message(FATAL_ERROR "brookesia_runtime_manager requires brookesia_service_manager to be enabled")
    endif()
    if(NOT TARGET brookesia::service_helper)
        message(FATAL_ERROR "brookesia_runtime_manager requires brookesia_service_helper to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::runtime_manager")
    add_subdirectory(runtime/brookesia_runtime_manager)
endif()

if(BROOKESIA_BUILD_RUNTIME_LUA)
    if(NOT TARGET brookesia::runtime_manager)
        message(FATAL_ERROR "brookesia_runtime_lua requires brookesia_runtime_manager to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::runtime_lua")
    add_subdirectory(runtime/brookesia_runtime_lua)
endif()

if(BROOKESIA_BUILD_RUNTIME_JS)
    if(NOT TARGET brookesia::runtime_manager)
        message(FATAL_ERROR "brookesia_runtime_js requires brookesia_runtime_manager to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::runtime_js")
    add_subdirectory(runtime/brookesia_runtime_js)
endif()

if(BROOKESIA_BUILD_RUNTIME_WASM)
    if(NOT TARGET brookesia::runtime_manager)
        message(FATAL_ERROR "brookesia_runtime_wasm requires brookesia_runtime_manager to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::runtime_wasm")
    add_subdirectory(runtime/brookesia_runtime_wasm)
endif()

if(BROOKESIA_BUILD_RUNTIME_ELF)
    message(FATAL_ERROR "brookesia_runtime_elf supports ESP-IDF builds only")
endif()

if(BROOKESIA_BUILD_SYSTEM_CORE)
    if(NOT TARGET brookesia::runtime_manager)
        message(FATAL_ERROR "brookesia_system_core requires brookesia_runtime_manager to be enabled")
    endif()
    if(NOT TARGET brookesia::gui_interface)
        message(FATAL_ERROR "brookesia_system_core requires brookesia_gui_interface to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::system_core")
    add_subdirectory(system/brookesia_system_core)
endif()

if(BROOKESIA_BUILD_SYSTEM_SUPER)
    if(NOT TARGET brookesia::system_core)
        message(FATAL_ERROR "brookesia_system_super requires brookesia_system_core to be enabled")
    endif()
    if(NOT TARGET brookesia::gui_lvgl)
        message(FATAL_ERROR "brookesia_system_super requires brookesia_gui_lvgl to be enabled")
    endif()

    message(STATUS "Brookesia: enabling brookesia::system_super")
    add_subdirectory(system/brookesia_system_super)
endif()

foreach(app_component IN LISTS BROOKESIA_SUPPORTED_APP_COMPONENTS)
    string(REGEX REPLACE "^brookesia_app_" "" app_name "${app_component}")
    string(TOUPPER "${app_name}" app_name_upper)
    string(REPLACE "-" "_" app_name_upper "${app_name_upper}")
    set(app_option "BROOKESIA_BUILD_APP_${app_name_upper}")
    if(${app_option})
        if(NOT TARGET brookesia::system_core)
            message(FATAL_ERROR "${app_component} requires brookesia_system_core to be enabled")
        endif()
        if(app_component STREQUAL "brookesia_app_calculator" AND NOT TARGET brookesia::runtime_js)
            message(WARNING "${app_component} requires brookesia_runtime_js; skipping")
            continue()
        endif()
        if(app_component STREQUAL "brookesia_app_2048" AND NOT TARGET brookesia::runtime_js)
            message(WARNING "${app_component} requires brookesia_runtime_js; skipping")
            continue()
        endif()
        if(app_component STREQUAL "brookesia_app_flappy_bird" AND NOT TARGET brookesia::runtime_js)
            message(WARNING "${app_component} requires brookesia_runtime_js; skipping")
            continue()
        endif()
        if(app_component STREQUAL "brookesia_app_flappy_bird" AND NOT TARGET brookesia::gui_lvgl)
            message(FATAL_ERROR "${app_component} requires brookesia_gui_lvgl to pack LVGL image resources")
        endif()
        if(app_component STREQUAL "brookesia_app_store" AND NOT TARGET brookesia::service_http)
            message(WARNING "${app_component} requires brookesia_service_http; skipping")
            continue()
        endif()
        if(app_component STREQUAL "brookesia_app_camera" AND NOT TARGET brookesia::runtime_js)
            message(WARNING "${app_component} requires brookesia_runtime_js; skipping")
            continue()
        endif()
        if(app_component STREQUAL "brookesia_app_camera" AND NOT TARGET brookesia::service_video)
            message(WARNING "${app_component} requires brookesia_service_video; skipping")
            continue()
        endif()
        if(app_component STREQUAL "brookesia_app_music_player" AND NOT TARGET brookesia::runtime_js)
            message(WARNING "${app_component} requires brookesia_runtime_js; skipping")
            continue()
        endif()
        if(app_component STREQUAL "brookesia_app_music_player" AND NOT TARGET brookesia::service_audio)
            message(WARNING "${app_component} requires brookesia_service_audio; skipping")
            continue()
        endif()
        if(app_component STREQUAL "brookesia_app_nes_emulator" AND NOT TARGET brookesia::runtime_js)
            message(WARNING "${app_component} requires brookesia_runtime_js; skipping")
            continue()
        endif()
        if(app_component STREQUAL "brookesia_app_nes_emulator" AND NOT TARGET brookesia::emulation_nes)
            message(WARNING "${app_component} requires brookesia_emulation_nes; skipping")
            continue()
        endif()
        if(app_component STREQUAL "brookesia_app_nes_emulator" AND NOT TARGET brookesia::service_display)
            message(WARNING "${app_component} requires brookesia_service_display; skipping")
            continue()
        endif()

        message(STATUS "Brookesia: enabling brookesia::app_${app_name}")
        add_subdirectory(app/${app_component})
    endif()
endforeach()

set(BROOKESIA_ALL_TARGET brookesia_all_impl)
add_library(${BROOKESIA_ALL_TARGET} INTERFACE)

if(TARGET brookesia::lib_utils)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::lib_utils)
endif()

if(TARGET brookesia::hal_interface)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::hal_interface)
endif()

if(TARGET brookesia::hal_wasm)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::hal_wasm)
endif()

if(TARGET brookesia::service_manager)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::service_manager)
endif()

if(TARGET brookesia::service_helper)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::service_helper)
endif()

if(TARGET brookesia::service_storage)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::service_storage)
endif()

if(TARGET brookesia::service_sntp)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::service_sntp)
endif()

if(TARGET brookesia::service_http)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::service_http)
endif()

if(TARGET brookesia::service_ota)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::service_ota)
endif()

if(TARGET brookesia::service_device)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::service_device)
endif()

if(TARGET brookesia::service_audio)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::service_audio)
endif()

if(TARGET brookesia::service_wifi)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::service_wifi)
endif()

if(TARGET brookesia::service_display)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::service_display)
endif()

if(TARGET brookesia::service_video)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::service_video)
endif()

if(TARGET brookesia::emulation_nes)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::emulation_nes)
endif()

if(TARGET brookesia::gui_interface)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::gui_interface)
endif()

if(TARGET brookesia::gui_lvgl)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::gui_lvgl)
endif()

if(TARGET brookesia::runtime_manager)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::runtime_manager)
endif()

if(TARGET brookesia::runtime_lua)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::runtime_lua)
endif()

if(TARGET brookesia::runtime_js)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::runtime_js)
endif()

if(TARGET brookesia::runtime_wasm)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::runtime_wasm)
endif()

if(TARGET brookesia::runtime_elf)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::runtime_elf)
endif()

if(TARGET brookesia::system_core)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::system_core)
endif()

if(TARGET brookesia::system_super)
    target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::system_super)
endif()

foreach(app_component IN LISTS BROOKESIA_SUPPORTED_APP_COMPONENTS)
    string(REGEX REPLACE "^brookesia_app_" "" app_name "${app_component}")
    if(TARGET brookesia::app_${app_name})
        target_link_libraries(${BROOKESIA_ALL_TARGET} INTERFACE brookesia::app_${app_name})
    endif()
endforeach()

add_library(brookesia::all ALIAS ${BROOKESIA_ALL_TARGET})
