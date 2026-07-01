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
    set(source_asset_dir "${COMPONENT_DIR}/assets")
    string(MD5 stage_cache_hash "${stage_super_root}_${stage_system_fonts_dir}_${stage_COPY_FONTS}")
    set(stage_image_cache_root "${CMAKE_BINARY_DIR}/brookesia_system_super_image_cache_${stage_cache_hash}")
    set(stage_image_cache_output_root "${stage_image_cache_root}/output")
    set(stage_image_merge_script "${stage_image_cache_root}/merge_image_indexes.py")

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

    file(REMOVE_RECURSE "${stage_image_cache_output_root}")
    set(has_source_asset_images FALSE)
    set(source_asset_image_files)
    if(EXISTS "${source_asset_dir}" AND IS_DIRECTORY "${source_asset_dir}")
        file(GLOB_RECURSE source_asset_image_files CONFIGURE_DEPENDS
            "${source_asset_dir}/*.png"
            "${source_asset_dir}/*.PNG"
            "${source_asset_dir}/*.jpg"
            "${source_asset_dir}/*.JPG"
            "${source_asset_dir}/*.jpeg"
            "${source_asset_dir}/*.JPEG"
        )
        list(SORT source_asset_image_files)
        if(source_asset_image_files)
            set(has_source_asset_images TRUE)
        else()
            message(STATUS "No PNG/JPG/JPEG images found under ${source_asset_dir}, skipping image packing")
        endif()
    else()
        message(STATUS "Brookesia system super image asset directory does not exist, skipping: ${source_asset_dir}")
    endif()

    set(image_ids)
    set(image_relative_paths)
    set(image_pack_keys)
    if(has_source_asset_images)
        if(NOT COMMAND brookesia_gui_lvgl_pack_images)
            message(FATAL_ERROR
                "brookesia_system_super_stage_resources requires brookesia_gui_lvgl_pack_images(). "
                "Make sure brookesia_gui_lvgl is available before brookesia_system_super."
            )
        endif()
        if(COMMAND brookesia_gui_lvgl_resolve_image_tool_python)
            brookesia_gui_lvgl_resolve_image_tool_python(stage_image_merge_python)
        elseif(ESP_PLATFORM AND DEFINED PYTHON AND NOT "${PYTHON}" STREQUAL "")
            set(stage_image_merge_python "${PYTHON}")
        else()
            find_package(Python3 REQUIRED COMPONENTS Interpreter)
            set(stage_image_merge_python "${Python3_EXECUTABLE}")
        endif()

        file(MAKE_DIRECTORY "${stage_image_cache_root}")
        file(WRITE "${stage_image_merge_script}" [=[
import json
import shutil
import sys
from pathlib import Path


def load_image_set(path):
    if not path.exists():
        return {"type": "imageSet", "images": []}
    with path.open("r", encoding="utf-8") as file:
        data = json.load(file)
    if data.get("type") != "imageSet" or not isinstance(data.get("images"), list):
        raise RuntimeError(f"Image descriptor is not an imageSet: {path}")
    return data


def main():
    if len(sys.argv) != 3:
        raise RuntimeError("Usage: merge_image_indexes.py <generated-root> <stage-root>")

    generated_root = Path(sys.argv[1])
    stage_root = Path(sys.argv[2])
    if not generated_root.is_dir():
        return 0

    for source in sorted(generated_root.rglob("*")):
        if not source.is_file() or source.name == "index.json":
            continue
        destination = stage_root / source.relative_to(generated_root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)

    for generated_index in sorted(generated_root.rglob("index.json")):
        relative_dir = generated_index.parent.relative_to(generated_root)
        staged_index = stage_root / relative_dir / "index.json"
        generated = load_image_set(generated_index)
        staged = load_image_set(staged_index)

        existing_ids = set()
        for image in staged["images"]:
            image_id = image.get("id")
            if not isinstance(image_id, str) or not image_id:
                raise RuntimeError(f"Invalid image id in staged descriptor: {staged_index}")
            existing_ids.add(image_id)

        for image in generated["images"]:
            image_id = image.get("id")
            if not isinstance(image_id, str) or not image_id:
                raise RuntimeError(f"Invalid image id in generated descriptor: {generated_index}")
            if image_id in existing_ids:
                raise RuntimeError(
                    f"Duplicate image id '{image_id}' while merging {generated_index} into {staged_index}"
                )
            staged["images"].append(image)
            existing_ids.add(image_id)

        staged_index.parent.mkdir(parents=True, exist_ok=True)
        temporary_index = staged_index.with_name(staged_index.name + ".tmp")
        with temporary_index.open("w", encoding="utf-8") as file:
            json.dump(staged, file, ensure_ascii=False, indent=4)
            file.write("\n")
        temporary_index.replace(staged_index)
    return 0


if __name__ == "__main__":
    sys.exit(main())
]=])

        foreach(image_file IN LISTS source_asset_image_files)
            get_filename_component(image_id "${image_file}" NAME_WE)
            if(image_id IN_LIST image_ids)
                message(FATAL_ERROR "Duplicate image id '${image_id}' from image file names under ${source_asset_dir}")
            endif()
            list(APPEND image_ids "${image_id}")

            file(RELATIVE_PATH image_relative_path "${source_asset_dir}" "${image_file}")
            if(image_relative_path IN_LIST image_relative_paths)
                message(FATAL_ERROR "Duplicate image relative path '${image_relative_path}' under ${source_asset_dir}")
            endif()
            list(APPEND image_relative_paths "${image_relative_path}")

            get_filename_component(image_relative_dir "${image_relative_path}" DIRECTORY)
            get_filename_component(image_ext "${image_file}" EXT)
            string(TOLOWER "${image_ext}" image_ext_lower)
            if(image_relative_dir STREQUAL "")
                set(image_relative_dir ".")
            endif()
            if(image_ext_lower STREQUAL ".png")
                set(image_format_group "rgb565a8")
            elseif(image_ext_lower STREQUAL ".jpg" OR image_ext_lower STREQUAL ".jpeg")
                set(image_format_group "rgb565")
            else()
                message(FATAL_ERROR "Unsupported image file extension '${image_ext}' for ${image_file}")
            endif()

            set(image_pack_key "${image_format_group}::${image_relative_dir}")
            list(APPEND image_pack_keys "${image_pack_key}")
            string(MD5 image_pack_key_hash "${image_pack_key}")
            set(image_pack_files_var "image_pack_files_${image_pack_key_hash}")
            list(APPEND ${image_pack_files_var} "${image_file}")
        endforeach()
        list(REMOVE_DUPLICATES image_pack_keys)

        foreach(image_pack_key IN LISTS image_pack_keys)
            string(REPLACE "::" ";" image_pack_parts "${image_pack_key}")
            list(GET image_pack_parts 0 image_format_group)
            list(GET image_pack_parts 1 image_relative_dir)
            string(MD5 image_pack_key_hash "${image_pack_key}")
            set(image_pack_files_var "image_pack_files_${image_pack_key_hash}")
            set(image_pack_files "${${image_pack_files_var}}")
            if(image_relative_dir STREQUAL ".")
                set(image_input_dir "${source_asset_dir}")
                set(image_output_cache_dir "${stage_image_cache_output_root}")
            else()
                set(image_input_dir "${source_asset_dir}/${image_relative_dir}")
                set(image_output_cache_dir "${stage_image_cache_output_root}/${image_relative_dir}")
            endif()

            if(image_format_group STREQUAL "rgb565a8")
                # Pass the real source image list into the packer. Do not copy images
                # into a configure-generated input cache, because CMake glob re-checks
                # on such a cache can trigger endless reconfigure loops.
                brookesia_gui_lvgl_pack_images(
                    INPUT_DIR "${image_input_dir}"
                    OUTPUT_DIR "${image_output_cache_dir}"
                    IMAGE_FILES ${image_pack_files}
                    CF RGB565A8
                )
            elseif(image_format_group STREQUAL "rgb565")
                brookesia_gui_lvgl_pack_images(
                    INPUT_DIR "${image_input_dir}"
                    OUTPUT_DIR "${image_output_cache_dir}"
                    IMAGE_FILES ${image_pack_files}
                    CF RGB565
                    RGB565_DITHER
                )
            else()
                message(FATAL_ERROR "Unknown image pack format group: ${image_format_group}")
            endif()
        endforeach()

        execute_process(
            COMMAND "${stage_image_merge_python}" "${stage_image_merge_script}"
                "${stage_image_cache_output_root}" "${stage_super_root}"
            RESULT_VARIABLE image_merge_result
            OUTPUT_VARIABLE image_merge_output
            ERROR_VARIABLE image_merge_error
        )
        if(NOT image_merge_result EQUAL 0)
            message(FATAL_ERROR
                "Failed to merge packed system super image descriptors: ${image_merge_output}${image_merge_error}"
            )
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
            if(has_source_asset_images)
                list(APPEND stage_commands
                    COMMAND "${stage_image_merge_python}" "${stage_image_merge_script}"
                        "${stage_image_cache_output_root}" "${stage_super_root}"
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
