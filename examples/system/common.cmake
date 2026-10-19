# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Include before IDF's project.cmake: its first dependency scan already reads SDKCONFIG.
# A new output starts from the user's configuration without modifying that input.
get_filename_component(system_sdkconfig "${SDKCONFIG}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT SDKCONFIG OR system_sdkconfig STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig")
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig" CACHE FILEPATH "Build-local system configuration" FORCE)
    if(NOT EXISTS "${SDKCONFIG}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig" AND
            NOT SDKCONFIG_DEFAULTS AND NOT DEFINED ENV{SDKCONFIG_DEFAULTS})
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig" "${SDKCONFIG}" COPYONLY)
    endif()
endif()

# esp_board_manager's CLI hook checks only <project>/sdkconfig. A new build-local
# config also needs the generated board defaults, in the same first position.
set(system_board_defaults "${CMAKE_CURRENT_SOURCE_DIR}/components/gen_bmgr_codes/board_manager.defaults")
if(EXISTS "${system_board_defaults}")
    set(system_defaults)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig.defaults")
        list(APPEND system_defaults sdkconfig.defaults)
    endif()
    list(APPEND system_defaults $ENV{SDKCONFIG_DEFAULTS} ${SDKCONFIG_DEFAULTS})
    set(system_defaults_absolute "${system_board_defaults}")
    foreach(default_file IN LISTS system_defaults)
        get_filename_component(default_file "${default_file}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND system_defaults_absolute "${default_file}")
    endforeach()
    list(REMOVE_DUPLICATES system_defaults_absolute)
    set(SDKCONFIG_DEFAULTS "${system_defaults_absolute}")
endif()

set(DEPENDENCIES_LOCK "${CMAKE_BINARY_DIR}/dependencies.lock")
if(NOT BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT)
    set(BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT "${CMAKE_BINARY_DIR}/littlefs_stage"
        CACHE PATH "Build-local filesystem resources" FORCE)
endif()
list(APPEND BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_PATHS_EXPLICIT_CACHE_VARS
    BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT)
list(REMOVE_DUPLICATES BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_PATHS_EXPLICIT_CACHE_VARS)
set(BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_PATHS_EXPLICIT_CACHE_VARS
    "${BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_PATHS_EXPLICIT_CACHE_VARS}"
    CACHE INTERNAL "Project runtime path overrides" FORCE)

# IDF's manager installs into <project_dir>/managed_components. Relocate only
# dependency state; all manifests and sources stay original.
set(system_component_wrapper "${CMAKE_CURRENT_LIST_DIR}/common/component_manager.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${system_component_wrapper}")
if(DEFINED ENV{IDF_COMPONENT_WRAPPER} AND NOT "$ENV{IDF_COMPONENT_WRAPPER}" STREQUAL "" AND
        NOT "$ENV{IDF_COMPONENT_WRAPPER}" STREQUAL "${system_component_wrapper}")
    message(FATAL_ERROR "System output isolation requires its IDF_COMPONENT_WRAPPER; an external wrapper is set")
endif()
set(ENV{IDF_COMPONENT_WRAPPER} "${system_component_wrapper}")

# IDF 6.0 has no wrapper hook and quotes PYTHON in some commands, so a command
# list cannot be used. Keep its interpreter cache intact for idf.py's environment
# check; use a build-local executable that forwards all unrelated Python calls.
if(EXISTS "$ENV{IDF_PATH}/tools/cmake/build.cmake")
    file(STRINGS "$ENV{IDF_PATH}/tools/cmake/build.cmake" system_wrapper_hook REGEX "IDF_COMPONENT_WRAPPER")
    if(NOT system_wrapper_hook)
        if(NOT PYTHON)
            find_program(system_python NAMES python python3 REQUIRED)
        else()
            set(system_python "${PYTHON}")
        endif()
        set(system_python_launcher "${CMAKE_CURRENT_LIST_DIR}/common/python_launcher.py")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${system_python_launcher}")
        execute_process(COMMAND "${system_python}" "${system_python_launcher}"
            --create "${CMAKE_BINARY_DIR}/system_python" "${system_python}"
            OUTPUT_VARIABLE system_python_command OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE system_python_result ERROR_VARIABLE system_python_error)
        if(NOT system_python_result EQUAL 0)
            message(FATAL_ERROR "Failed to prepare IDF Python launcher: ${system_python_error}")
        endif()
        set(PYTHON "${system_python_command}")
        message(STATUS "System dependency isolation: legacy IDF Python launcher ${PYTHON}")
    endif()
endif()

# Component resolution precedes the final Kconfig pass in project(). Defer the
# success fingerprint until the directory finishes configuring, including users
# of this common file outside the two system examples.
function(brookesia_system_finalize_dependencies)
    idf_build_get_property(python PYTHON)
    idf_build_get_property(lock_file DEPENDENCIES_LOCK)
    execute_process(COMMAND "${python}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/common/component_manager.py"
        --finalize "${lock_file}"
        RESULT_VARIABLE result ERROR_VARIABLE errors)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Failed to record final dependency state: ${errors}")
    endif()
endfunction()
cmake_language(DEFER CALL brookesia_system_finalize_dependencies)

function(brookesia_system_check_build_backend)
    if(CONFIG_BROOKESIA_GUI_BACKEND_GSP)
        set(backend gsp)
    else()
        set(backend lvgl)
    endif()
    if(DEFINED BROOKESIA_SYSTEM_BUILD_BACKEND AND NOT BROOKESIA_SYSTEM_BUILD_BACKEND STREQUAL backend)
        message(FATAL_ERROR
            "This output belongs to ${BROOKESIA_SYSTEM_BUILD_BACKEND}; use a separate -B directory for ${backend}")
    endif()
    set(BROOKESIA_SYSTEM_BUILD_BACKEND "${backend}" CACHE INTERNAL "Backend owning this output")
endfunction()
