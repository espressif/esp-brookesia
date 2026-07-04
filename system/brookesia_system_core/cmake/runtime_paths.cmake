# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

set(BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_INTERNAL_ROOT_DEFAULT "/littlefs")
set(BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_PATHS_EXPLICIT_CACHE_VARS_CACHE
    BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_PATHS_EXPLICIT_CACHE_VARS
)

function(_brookesia_system_core_normalize_runtime_path output_variable runtime_path)
    if(NOT runtime_path)
        message(FATAL_ERROR "Runtime path must not be empty")
    endif()
    if(NOT runtime_path MATCHES "^/")
        message(FATAL_ERROR "Runtime path must be absolute: ${runtime_path}")
    endif()
    string(REGEX REPLACE "/+$" "" normalized_path "${runtime_path}")
    if(normalized_path STREQUAL "")
        set(normalized_path "/")
    endif()
    set(${output_variable} "${normalized_path}" PARENT_SCOPE)
endfunction()

function(_brookesia_system_core_strip_config_string output_variable value)
    string(REGEX REPLACE "^\"(.*)\"$" "\\1" stripped_value "${value}")
    set(${output_variable} "${stripped_value}" PARENT_SCOPE)
endfunction()

function(_brookesia_system_core_get_config_string output_variable config_name fallback_value)
    if(DEFINED ${config_name})
        _brookesia_system_core_strip_config_string(config_value "${${config_name}}")
        set(${output_variable} "${config_value}" PARENT_SCOPE)
    else()
        set(${output_variable} "${fallback_value}" PARENT_SCOPE)
    endif()
endfunction()

function(_brookesia_system_core_cache_variable_is_explicit output_variable variable_name)
    get_property(cache_variable_set CACHE ${variable_name} PROPERTY VALUE SET)
    if(NOT cache_variable_set)
        set(${output_variable} FALSE PARENT_SCOPE)
        return()
    endif()

    get_property(
        explicit_cache_vars_set
        CACHE ${BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_PATHS_EXPLICIT_CACHE_VARS_CACHE}
        PROPERTY VALUE
        SET
    )
    if(explicit_cache_vars_set)
        get_property(
            explicit_cache_vars
            CACHE ${BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_PATHS_EXPLICIT_CACHE_VARS_CACHE}
            PROPERTY VALUE
        )
        if("${variable_name}" IN_LIST explicit_cache_vars)
            set(${output_variable} TRUE PARENT_SCOPE)
        else()
            set(${output_variable} FALSE PARENT_SCOPE)
        endif()
        return()
    endif()

    # A cache value with no marker usually comes from -DVAR=... or an older build
    # tree. Treat it as explicit so existing project overrides keep working.
    set(${output_variable} TRUE PARENT_SCOPE)
endfunction()

function(_brookesia_system_core_resolve_runtime_path output_variable variable_name config_name fallback_value)
    _brookesia_system_core_cache_variable_is_explicit(cache_variable_explicit "${variable_name}")
    if(cache_variable_explicit)
        get_property(path_value CACHE ${variable_name} PROPERTY VALUE)
    else()
        _brookesia_system_core_get_config_string(path_value "${config_name}" "${fallback_value}")
    endif()

    _brookesia_system_core_normalize_runtime_path(normalized_path "${path_value}")
    set(${output_variable} "${normalized_path}" PARENT_SCOPE)
    set(${output_variable}_EXPLICIT "${cache_variable_explicit}" PARENT_SCOPE)
endfunction()

function(_brookesia_system_core_resolve_stage_root output_variable variable_name config_name fallback_value)
    _brookesia_system_core_cache_variable_is_explicit(cache_variable_explicit "${variable_name}")
    if(cache_variable_explicit)
        get_property(path_value CACHE ${variable_name} PROPERTY VALUE)
    else()
        _brookesia_system_core_get_config_string(path_value "${config_name}" "${fallback_value}")
    endif()

    set(${output_variable} "${path_value}" PARENT_SCOPE)
    set(${output_variable}_EXPLICIT "${cache_variable_explicit}" PARENT_SCOPE)
endfunction()

function(brookesia_system_core_apply_esp_runtime_paths)
    _brookesia_system_core_resolve_runtime_path(
        internal_root
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_INTERNAL_ROOT
        CONFIG_BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_INTERNAL_ROOT
        "${BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_INTERNAL_ROOT_DEFAULT}"
    )

    _brookesia_system_core_resolve_stage_root(
        app_root_config
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_APP_ROOT
        CONFIG_BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_APP_ROOT
        ""
    )
    if(app_root_config)
        _brookesia_system_core_normalize_runtime_path(app_root "${app_root_config}")
    else()
        set(app_root "${internal_root}/apps")
    endif()

    _brookesia_system_core_resolve_stage_root(
        system_root_config
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_SYSTEM_ROOT
        CONFIG_BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_SYSTEM_ROOT
        ""
    )
    if(system_root_config)
        _brookesia_system_core_normalize_runtime_path(system_root "${system_root_config}")
    else()
        set(system_root "${internal_root}/system")
    endif()

    _brookesia_system_core_resolve_stage_root(
        resource_stage_root
        BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT
        CONFIG_BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT
        ""
    )

    set(explicit_cache_vars)
    foreach(path_variable IN ITEMS
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_INTERNAL_ROOT
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_APP_ROOT
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_SYSTEM_ROOT
        BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT
    )
        _brookesia_system_core_cache_variable_is_explicit(cache_variable_explicit "${path_variable}")
        if(cache_variable_explicit)
            list(APPEND explicit_cache_vars "${path_variable}")
        endif()
    endforeach()

    set(
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_INTERNAL_ROOT
        "${internal_root}"
        CACHE STRING
        "ESP runtime internal storage root"
        FORCE
    )
    set(
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_APP_ROOT
        "${app_root}"
        CACHE STRING
        "ESP runtime app root"
        FORCE
    )
    set(
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_SYSTEM_ROOT
        "${system_root}"
        CACHE STRING
        "ESP runtime system resource root"
        FORCE
    )
    set(
        BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT
        "${resource_stage_root}"
        CACHE PATH
        "Override ESP host staging root for system_core runtime roots"
        FORCE
    )
    set(
        ${BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_PATHS_EXPLICIT_CACHE_VARS_CACHE}
        "${explicit_cache_vars}"
        CACHE INTERNAL
        "ESP runtime path cache variables explicitly set by the project"
        FORCE
    )

    set(BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_INTERNAL_ROOT "${internal_root}" PARENT_SCOPE)
    set(BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_APP_ROOT "${app_root}" PARENT_SCOPE)
    set(BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_SYSTEM_ROOT "${system_root}" PARENT_SCOPE)
    set(BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT "${resource_stage_root}" PARENT_SCOPE)
endfunction()

function(brookesia_system_core_set_esp_runtime_paths)
    set(options)
    set(one_value_args INTERNAL_ROOT APP_ROOT SYSTEM_ROOT RESOURCE_STAGE_ROOT)
    set(multi_value_args)
    cmake_parse_arguments(paths "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(paths_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown arguments: ${paths_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT paths_INTERNAL_ROOT)
        message(FATAL_ERROR "brookesia_system_core_set_esp_runtime_paths requires INTERNAL_ROOT")
    endif()

    _brookesia_system_core_normalize_runtime_path(internal_root "${paths_INTERNAL_ROOT}")
    if(paths_APP_ROOT)
        _brookesia_system_core_normalize_runtime_path(app_root "${paths_APP_ROOT}")
    else()
        set(app_root "${internal_root}/apps")
    endif()
    if(paths_SYSTEM_ROOT)
        _brookesia_system_core_normalize_runtime_path(system_root "${paths_SYSTEM_ROOT}")
    else()
        set(system_root "${internal_root}/system")
    endif()

    set(
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_INTERNAL_ROOT
        "${internal_root}"
        CACHE STRING
        "ESP runtime internal storage root"
        FORCE
    )
    set(
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_APP_ROOT
        "${app_root}"
        CACHE STRING
        "ESP runtime app root"
        FORCE
    )
    set(
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_SYSTEM_ROOT
        "${system_root}"
        CACHE STRING
        "ESP runtime system resource root"
        FORCE
    )
    set(explicit_cache_vars
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_INTERNAL_ROOT
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_APP_ROOT
        BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_SYSTEM_ROOT
    )
    if(paths_RESOURCE_STAGE_ROOT)
        set(
            BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT
            "${paths_RESOURCE_STAGE_ROOT}"
            CACHE PATH
            "Override ESP host staging root for system_core runtime roots"
            FORCE
        )
        list(APPEND explicit_cache_vars BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT)
    endif()
    set(
        ${BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_PATHS_EXPLICIT_CACHE_VARS_CACHE}
        "${explicit_cache_vars}"
        CACHE INTERNAL
        "ESP runtime path cache variables explicitly set by the project"
        FORCE
    )
endfunction()

function(brookesia_system_core_runtime_path_to_stage_dir output_variable runtime_path)
    if(NOT runtime_path)
        message(FATAL_ERROR "brookesia_system_core_runtime_path_to_stage_dir requires runtime_path")
    endif()

    if(ESP_PLATFORM AND DEFINED PROJECT_DIR AND runtime_path MATCHES "^/")
        string(REGEX REPLACE "^/+" "" normalized_path "${runtime_path}")
        if(normalized_path STREQUAL "")
            message(FATAL_ERROR "Invalid runtime root path: ${runtime_path}")
        endif()
        string(REPLACE "/" ";" normalized_path_parts "${normalized_path}")
        list(POP_FRONT normalized_path_parts mount_root_name)

        if(BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT)
            if(IS_ABSOLUTE "${BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT}")
                set(stage_mount_root_input "${BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT}")
            else()
                set(stage_mount_root_input "${PROJECT_DIR}/${BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT}")
            endif()
            get_filename_component(stage_mount_root "${stage_mount_root_input}" ABSOLUTE)
        else()
            set(stage_mount_root "${PROJECT_DIR}/${mount_root_name}")
        endif()

        if(normalized_path_parts)
            list(JOIN normalized_path_parts "/" relative_path)
            set(stage_dir "${stage_mount_root}/${relative_path}")
        else()
            set(stage_dir "${stage_mount_root}")
        endif()
    else()
        get_filename_component(stage_dir "${runtime_path}" ABSOLUTE)
    endif()

    set(${output_variable} "${stage_dir}" PARENT_SCOPE)
endfunction()

function(brookesia_system_core_get_esp_runtime_app_stage_root output_variable)
    brookesia_system_core_apply_esp_runtime_paths()
    brookesia_system_core_runtime_path_to_stage_dir(
        stage_root
        "${BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_APP_ROOT}"
    )
    set(${output_variable} "${stage_root}" PARENT_SCOPE)
endfunction()

function(brookesia_system_core_get_esp_runtime_system_stage_root output_variable)
    brookesia_system_core_apply_esp_runtime_paths()
    brookesia_system_core_runtime_path_to_stage_dir(
        stage_root
        "${BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_SYSTEM_ROOT}"
    )
    set(${output_variable} "${stage_root}" PARENT_SCOPE)
endfunction()
