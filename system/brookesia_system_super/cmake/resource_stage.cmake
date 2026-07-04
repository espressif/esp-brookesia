# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

function(brookesia_system_super_stage_resources)
    set(options)
    set(one_value_args STAGE_ROOT SYSTEM_STAGE_ROOT SHARE_STAGE_ROOT COPY_FONTS)
    set(multi_value_args)
    cmake_parse_arguments(stage "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT DEFINED stage_COPY_FONTS OR "${stage_COPY_FONTS}" STREQUAL "")
        set(stage_COPY_FONTS ON)
    endif()

    if(stage_STAGE_ROOT AND NOT stage_SYSTEM_STAGE_ROOT)
        set(stage_SYSTEM_STAGE_ROOT "${stage_STAGE_ROOT}")
    endif()
    if(NOT stage_SYSTEM_STAGE_ROOT)
        message(FATAL_ERROR "brookesia_system_super_stage_resources requires SYSTEM_STAGE_ROOT")
    endif()

    get_filename_component(system_stage_root "${stage_SYSTEM_STAGE_ROOT}" ABSOLUTE)
    set(legacy_cleanup_commands)
    get_filename_component(system_stage_root_name "${system_stage_root}" NAME)
    if(system_stage_root_name STREQUAL "system")
        get_filename_component(system_stage_mount_root "${system_stage_root}" DIRECTORY)
        file(REMOVE_RECURSE "${system_stage_mount_root}/super")
        file(REMOVE_RECURSE "${system_stage_root}/share")
        list(APPEND legacy_cleanup_commands
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${system_stage_mount_root}/super"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${system_stage_root}/share"
        )
    endif()
    if(stage_COPY_FONTS AND stage_SHARE_STAGE_ROOT)
        get_filename_component(share_stage_root "${stage_SHARE_STAGE_ROOT}" ABSOLUTE)
        get_filename_component(share_stage_root_name "${share_stage_root}" NAME)
        if(share_stage_root_name STREQUAL "share")
            get_filename_component(share_stage_mount_root "${share_stage_root}" DIRECTORY)
            file(REMOVE_RECURSE "${share_stage_mount_root}/fonts")
            list(APPEND legacy_cleanup_commands
                COMMAND ${CMAKE_COMMAND} -E remove_directory "${share_stage_mount_root}/fonts"
            )
        endif()
    endif()

    set(stage_super_root "${system_stage_root}/super")
    set(stage_system_fonts_dir "${system_stage_root}/fonts")
    set(source_shell_dir "${COMPONENT_DIR}/resource/shell")
    set(source_theme_dir "${COMPONENT_DIR}/resource/themes")
    set(source_font_dir "${COMPONENT_DIR}/resource/fonts")
    set(source_keyboard_icon_source_dir "${source_font_dir}/icon/source")
    set(source_startup_dir "${COMPONENT_DIR}/resource/startup")
    string(MD5 stage_cache_hash "${stage_super_root}_${stage_system_fonts_dir}_${stage_COPY_FONTS}")

    file(REMOVE_RECURSE "${stage_super_root}")
    file(MAKE_DIRECTORY "${stage_super_root}")
    file(COPY "${source_shell_dir}" DESTINATION "${stage_super_root}")
    file(COPY "${source_theme_dir}" DESTINATION "${stage_super_root}")
    file(COPY "${source_startup_dir}" DESTINATION "${stage_super_root}")
    if(EXISTS "${source_keyboard_icon_source_dir}" AND IS_DIRECTORY "${source_keyboard_icon_source_dir}")
        file(COPY "${source_keyboard_icon_source_dir}" DESTINATION "${stage_super_root}/shell/images/keyboard")
    endif()

    if(stage_COPY_FONTS)
        file(REMOVE_RECURSE "${stage_system_fonts_dir}")
        if(EXISTS "${source_font_dir}" AND IS_DIRECTORY "${source_font_dir}")
            file(MAKE_DIRECTORY "${system_stage_root}")
            file(COPY "${source_font_dir}" DESTINATION "${system_stage_root}")
        endif()
    endif()

    if(NOT CMAKE_SCRIPT_MODE_FILE)
        set(stage_target_hash "${stage_cache_hash}")
        set(stage_target "brookesia_system_super_stage_resources_${stage_target_hash}")
        if(NOT TARGET ${stage_target})
            set(stage_commands
                ${legacy_cleanup_commands}
                COMMAND ${CMAKE_COMMAND} -E remove_directory "${stage_super_root}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_super_root}"
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${source_shell_dir}" "${stage_super_root}/shell"
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${source_theme_dir}" "${stage_super_root}/themes"
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${source_startup_dir}" "${stage_super_root}/startup"
            )
            if(EXISTS "${source_keyboard_icon_source_dir}" AND IS_DIRECTORY "${source_keyboard_icon_source_dir}")
                list(APPEND stage_commands
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${source_keyboard_icon_source_dir}" "${stage_super_root}/shell/images/keyboard/source"
                )
            endif()
            if(stage_COPY_FONTS)
                list(APPEND stage_commands
                    COMMAND ${CMAKE_COMMAND} -E remove_directory "${stage_system_fonts_dir}"
                )
            endif()
            if(stage_COPY_FONTS AND EXISTS "${source_font_dir}" AND IS_DIRECTORY "${source_font_dir}")
                list(APPEND stage_commands
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${system_stage_root}"
                    COMMAND ${CMAKE_COMMAND} -E copy_directory "${source_font_dir}" "${stage_system_fonts_dir}"
                )
            endif()
            add_custom_target(${stage_target} ALL
                ${stage_commands}
                COMMENT "Staging Brookesia system super resources"
                VERBATIM
            )
        endif()

        get_property(stage_targets GLOBAL PROPERTY BROOKESIA_SYSTEM_SUPER_STAGE_TARGETS)
        list(APPEND stage_targets "${stage_target}")
        list(REMOVE_DUPLICATES stage_targets)
        set_property(GLOBAL PROPERTY BROOKESIA_SYSTEM_SUPER_STAGE_TARGETS "${stage_targets}")
    endif()

    set(BROOKESIA_SYSTEM_SUPER_SYSTEM_STAGE_ROOT "${system_stage_root}" PARENT_SCOPE)
endfunction()
