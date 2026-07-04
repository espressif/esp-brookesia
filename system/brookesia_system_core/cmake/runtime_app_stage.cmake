# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

include(${CMAKE_CURRENT_LIST_DIR}/runtime_paths.cmake)
brookesia_system_core_apply_esp_runtime_paths()

function(brookesia_system_core_cleanup_runtime_stage_root stage_root)
    string(MD5 stage_key "${stage_root}")
    get_property(stage_cleaned GLOBAL PROPERTY "BROOKESIA_RUNTIME_APP_STAGE_CLEANED_${stage_key}")
    if(stage_cleaned)
        return()
    endif()

    file(REMOVE_RECURSE "${stage_root}")
    file(MAKE_DIRECTORY "${stage_root}")

    get_filename_component(stage_root_name "${stage_root}" NAME)
    set(legacy_cleanup_commands)
    if(stage_root_name STREQUAL "app")
        get_filename_component(stage_mount_root "${stage_root}" DIRECTORY)
        file(REMOVE "${stage_mount_root}/app_index.json")
        file(GLOB legacy_app_dirs CONFIGURE_DEPENDS "${stage_mount_root}/brookesia.app.*")
        foreach(legacy_app_dir IN LISTS legacy_app_dirs)
            if(IS_DIRECTORY "${legacy_app_dir}")
                file(REMOVE_RECURSE "${legacy_app_dir}")
            endif()
        endforeach()
    elseif(stage_root_name STREQUAL "apps")
        get_filename_component(stage_mount_root "${stage_root}" DIRECTORY)
        file(REMOVE_RECURSE "${stage_mount_root}/app")
        file(REMOVE "${stage_mount_root}/app_index.json")
        list(APPEND legacy_cleanup_commands
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${stage_mount_root}/app"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${stage_mount_root}/app_index.json"
        )
    endif()

    set_property(GLOBAL PROPERTY "BROOKESIA_RUNTIME_APP_STAGE_LEGACY_CLEANUP_${stage_key}" "${legacy_cleanup_commands}")
    set_property(GLOBAL PROPERTY "BROOKESIA_RUNTIME_APP_STAGE_CLEANED_${stage_key}" TRUE)
endfunction()

function(brookesia_stage_runtime_app_package)
    set(options NO_INDEX)
    set(one_value_args PACKAGE_ID SOURCE_DIR STAGE_ROOT)
    set(multi_value_args)
    cmake_parse_arguments(stage "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT stage_PACKAGE_ID)
        message(FATAL_ERROR "brookesia_stage_runtime_app_package requires PACKAGE_ID")
    endif()
    if(NOT stage_SOURCE_DIR)
        message(FATAL_ERROR "brookesia_stage_runtime_app_package requires SOURCE_DIR")
    endif()
    if(NOT stage_STAGE_ROOT)
        message(FATAL_ERROR "brookesia_stage_runtime_app_package requires STAGE_ROOT")
    endif()

    get_filename_component(stage_source_dir "${stage_SOURCE_DIR}" ABSOLUTE)
    get_filename_component(stage_root "${stage_STAGE_ROOT}" ABSOLUTE)
    set(stage_dir "${stage_root}/${stage_PACKAGE_ID}")

    brookesia_system_core_cleanup_runtime_stage_root("${stage_root}")
    file(REMOVE_RECURSE "${stage_dir}")
    file(COPY "${stage_source_dir}/" DESTINATION "${stage_dir}")

    string(MD5 stage_target_hash "${stage_dir}")
    string(MAKE_C_IDENTIFIER "${stage_PACKAGE_ID}" stage_target_id)
    set(stage_target "brookesia_stage_runtime_app_${stage_target_id}_${stage_target_hash}")
    if(NOT TARGET ${stage_target})
        string(MD5 stage_key "${stage_root}")
        get_property(legacy_cleanup_commands GLOBAL PROPERTY "BROOKESIA_RUNTIME_APP_STAGE_LEGACY_CLEANUP_${stage_key}")
        add_custom_target(${stage_target} ALL
            ${legacy_cleanup_commands}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_root}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${stage_source_dir}" "${stage_dir}"
            COMMENT "Staging Brookesia runtime app package ${stage_PACKAGE_ID}"
            VERBATIM
        )
    endif()

    get_property(stage_targets GLOBAL PROPERTY BROOKESIA_RUNTIME_APP_STAGE_TARGETS)
    list(APPEND stage_targets "${stage_target}")
    list(REMOVE_DUPLICATES stage_targets)
    set_property(GLOBAL PROPERTY BROOKESIA_RUNTIME_APP_STAGE_TARGETS "${stage_targets}")
endfunction()
