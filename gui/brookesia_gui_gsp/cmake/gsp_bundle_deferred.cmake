# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)

set(BROOKESIA_GUI_GSP_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

# brookesia_gsp_add_bundle(<component_lib> <gsp_add_bundle arguments>...)
#
# Same arguments as esp-gsp's gsp_add_bundle(). The scene compiler runs at
# build time, so menuconfig still opens when it is not installed.
function(brookesia_gsp_add_bundle target)
    if(NOT COMMAND gsp_add_bundle)
        message(FATAL_ERROR "brookesia_gsp_add_bundle: esp-gsp is not part of this build")
    endif()

    idf_build_get_property(build_components BUILD_COMPONENTS)
    set(gsp_root "")
    foreach(component_name IN LISTS build_components)
        if(component_name MATCHES "esp-gsp$")
            idf_component_get_property(gsp_root ${component_name} COMPONENT_DIR)
        endif()
    endforeach()
    if(NOT gsp_root)
        message(FATAL_ERROR "brookesia_gsp_add_bundle: esp-gsp component directory was not found")
    endif()

    get_property(gspc_command GLOBAL PROPERTY ESP_GSPC_COMMAND)
    if(NOT gspc_command)
        set(build_step "${BROOKESIA_GUI_GSP_CMAKE_DIR}/gspc_build_step.cmake")
        set(step_args
            "-DESP_GSP_ROOT=${gsp_root}"
            "-DPROJECT_DIR=${PROJECT_DIR}"
        )
        if(DEFINED PYTHON AND NOT "${PYTHON}" STREQUAL "")
            list(APPEND step_args "-DPYTHON=${PYTHON}")
        endif()
        set(gspc_file "")
        if(DEFINED GSPC_EXECUTABLE AND NOT "${GSPC_EXECUTABLE}" STREQUAL "")
            set(gspc_file "${GSPC_EXECUTABLE}")
        elseif(DEFINED ENV{GSPC_EXECUTABLE} AND NOT "$ENV{GSPC_EXECUTABLE}" STREQUAL "")
            set(gspc_file "$ENV{GSPC_EXECUTABLE}")
        endif()
        if(NOT "${gspc_file}" STREQUAL "")
            list(APPEND step_args "-DGSPC_EXECUTABLE=${gspc_file}")
        endif()
        if(DEFINED CONFIG_ESP_GSP_ENABLE_JPEG)
            if(CONFIG_ESP_GSP_ENABLE_JPEG)
                list(APPEND step_args "-DCONFIG_ESP_GSP_ENABLE_JPEG=1")
            else()
                list(APPEND step_args "-DCONFIG_ESP_GSP_ENABLE_JPEG=0")
            endif()
        endif()

        # Rebuild the bundle when the compiler binary or its sources change.
        set(step_dependencies "${build_step}")
        if(NOT "${gspc_file}" STREQUAL "" AND EXISTS "${gspc_file}")
            list(APPEND step_dependencies "${gspc_file}")
        endif()
        foreach(version_file IN ITEMS "${PROJECT_DIR}/.gspc_version" "${gsp_root}/.gspc_version")
            if(EXISTS "${version_file}")
                list(APPEND step_dependencies "${version_file}")
            endif()
        endforeach()
        if("${gspc_file}" STREQUAL "")
            unset(BROOKESIA_GSPC_ON_PATH CACHE)
            find_program(BROOKESIA_GSPC_ON_PATH NAMES gspc NO_CACHE)
            if(BROOKESIA_GSPC_ON_PATH AND EXISTS "${BROOKESIA_GSPC_ON_PATH}")
                list(APPEND step_dependencies "${BROOKESIA_GSPC_ON_PATH}")
            endif()
            if(EXISTS "${gsp_root}/ci/gspc-dev")
                list(APPEND step_dependencies "${gsp_root}/ci/gspc-dev")
                file(GLOB_RECURSE gspc_source_deps CONFIGURE_DEPENDS
                    "${gsp_root}/tools/gspc/*.rs"
                    "${gsp_root}/tools/gspc/Cargo.toml"
                    "${gsp_root}/tools/gspc/Cargo.lock"
                    "${gsp_root}/tools/gspc/*/Cargo.toml"
                    "${gsp_root}/tools/gspc/*/build.rs"
                )
                list(APPEND step_dependencies ${gspc_source_deps}
                    "${gsp_root}/formats/gsp_format.yaml"
                )
            endif()
        endif()

        # Hand esp-gsp a build-time command so it does not look up the compiler now.
        set_property(GLOBAL PROPERTY ESP_GSPC_COMMAND
            "${CMAKE_COMMAND};${step_args};-P;${build_step};--")
        set_property(GLOBAL PROPERTY ESP_GSPC_DEPENDENCIES "${step_dependencies}")
        message(STATUS "brookesia_gui_gsp: GSPC is resolved when the bundle is built")
    endif()

    gsp_add_bundle(${target} ${ARGN})
endfunction()
