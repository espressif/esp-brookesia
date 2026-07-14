include_guard(GLOBAL)

function(brookesia_parse_component_version component_dir output_prefix)
    if("${component_dir}" STREQUAL "")
        message(FATAL_ERROR "Component directory must not be empty")
    endif()
    if("${output_prefix}" STREQUAL "")
        message(FATAL_ERROR "Component version output prefix must not be empty")
    endif()

    set(component_version_file "${component_dir}/idf_component.yml")
    if(NOT EXISTS "${component_version_file}")
        message(FATAL_ERROR "Component version file does not exist: ${component_version_file}")
    endif()

    file(
        STRINGS "${component_version_file}" component_version_lines
        REGEX "^[ \t]*version:[ \t]*\"?[0-9]+\\.[0-9]+\\.[0-9]+\"?[ \t]*$"
    )
    list(LENGTH component_version_lines component_version_count)
    if(NOT component_version_count EQUAL 1)
        message(FATAL_ERROR "Failed to parse component version from ${component_version_file}")
    endif()

    list(GET component_version_lines 0 component_version_line)
    string(
        REGEX REPLACE
        "^[ \t]*version:[ \t]*\"?([0-9]+)\\.([0-9]+)\\.([0-9]+)\"?[ \t]*$"
        "\\1;\\2;\\3"
        component_version_parts
        "${component_version_line}"
    )
    list(GET component_version_parts 0 component_version_major)
    list(GET component_version_parts 1 component_version_minor)
    list(GET component_version_parts 2 component_version_patch)

    set(
        ${output_prefix}
        "${component_version_major}.${component_version_minor}.${component_version_patch}"
        PARENT_SCOPE
    )
    set(${output_prefix}_MAJOR "${component_version_major}" PARENT_SCOPE)
    set(${output_prefix}_MINOR "${component_version_minor}" PARENT_SCOPE)
    set(${output_prefix}_PATCH "${component_version_patch}" PARENT_SCOPE)
endfunction()

function(brookesia_define_component_version target component_dir macro_prefix)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Component target does not exist: ${target}")
    endif()
    if("${macro_prefix}" STREQUAL "")
        message(FATAL_ERROR "Component version macro prefix must not be empty")
    endif()

    brookesia_parse_component_version("${component_dir}" component_version)
    get_target_property(component_target_type ${target} TYPE)
    if(component_target_type STREQUAL "INTERFACE_LIBRARY")
        set(component_version_scope INTERFACE)
    else()
        set(component_version_scope PRIVATE)
    endif()

    target_compile_definitions(
        ${target} ${component_version_scope}
        "${macro_prefix}_VER_MAJOR=(${component_version_MAJOR})"
        "${macro_prefix}_VER_MINOR=(${component_version_MINOR})"
        "${macro_prefix}_VER_PATCH=(${component_version_PATCH})"
    )
endfunction()
