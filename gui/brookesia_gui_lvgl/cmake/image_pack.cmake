# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

set(BROOKESIA_GUI_LVGL_IMAGE_TOOL "" CACHE FILEPATH
    "Override path to LVGL scripts/LVGLImage.py for brookesia_gui_lvgl image packing"
)
set(BROOKESIA_GUI_LVGL_IMAGE_TOOL_PYTHON "" CACHE FILEPATH
    "Override Python executable used by brookesia_gui_lvgl image packing"
)
option(
    BROOKESIA_GUI_LVGL_IMAGE_TOOL_AUTO_INSTALL_PIP_DEPS
    "Automatically install LVGLImage.py Python dependencies into a build-local virtual environment"
    ON
)

function(brookesia_gui_lvgl_resolve_image_tool out_var)
    if(BROOKESIA_GUI_LVGL_IMAGE_TOOL)
        set(image_tool "${BROOKESIA_GUI_LVGL_IMAGE_TOOL}")
    elseif(ESP_PLATFORM AND COMMAND idf_component_get_property)
        idf_build_get_property(build_components BUILD_COMPONENTS)
        set(lvgl_component "")
        if("lvgl" IN_LIST build_components)
            set(lvgl_component "lvgl")
        elseif("lvgl__lvgl" IN_LIST build_components)
            set(lvgl_component "lvgl__lvgl")
        endif()
        if(lvgl_component)
            idf_component_get_property(lvgl_dir ${lvgl_component} COMPONENT_DIR)
            set(image_tool "${lvgl_dir}/scripts/LVGLImage.py")
        else()
            set(image_tool "")
        endif()
    elseif(TARGET lvgl)
        get_target_property(lvgl_dir lvgl SOURCE_DIR)
        set(image_tool "${lvgl_dir}/scripts/LVGLImage.py")
    else()
        set(image_tool "")
    endif()

    if(NOT image_tool OR NOT EXISTS "${image_tool}")
        message(FATAL_ERROR
            "Failed to resolve LVGLImage.py. Set BROOKESIA_GUI_LVGL_IMAGE_TOOL or make sure the lvgl component "
            "is available before calling brookesia_gui_lvgl_pack_images()."
        )
    endif()

    set(${out_var} "${image_tool}" PARENT_SCOPE)
endfunction()

function(brookesia_gui_lvgl_python_has_image_deps python_executable out_var out_error_var)
    execute_process(
        COMMAND "${python_executable}" -c "import png; import lz4.block; import PIL.Image"
        RESULT_VARIABLE image_tool_deps_result
        ERROR_VARIABLE image_tool_deps_error
        OUTPUT_QUIET
    )
    if(image_tool_deps_result EQUAL 0)
        set(${out_var} true PARENT_SCOPE)
    else()
        set(${out_var} false PARENT_SCOPE)
    endif()
    set(${out_error_var} "${image_tool_deps_error}" PARENT_SCOPE)
endfunction()

function(brookesia_gui_lvgl_resolve_image_tool_python out_var)
    set(python_candidates)
    if(BROOKESIA_GUI_LVGL_IMAGE_TOOL_PYTHON)
        list(APPEND python_candidates "${BROOKESIA_GUI_LVGL_IMAGE_TOOL_PYTHON}")
    elseif(ESP_PLATFORM AND DEFINED PYTHON AND NOT "${PYTHON}" STREQUAL "")
        list(APPEND python_candidates "${PYTHON}")
    else()
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        list(APPEND python_candidates "${Python3_EXECUTABLE}")
        if(DEFINED BROOKESIA_ROOT_DIR)
            list(APPEND python_candidates "${BROOKESIA_ROOT_DIR}/.venv-docs/bin/python")
            list(APPEND python_candidates "${BROOKESIA_ROOT_DIR}/.venv/bin/python")
        endif()
    endif()
    list(REMOVE_DUPLICATES python_candidates)

    set(first_dependency_error "")
    foreach(python_candidate IN LISTS python_candidates)
        if(NOT EXISTS "${python_candidate}")
            continue()
        endif()
        brookesia_gui_lvgl_python_has_image_deps(
            "${python_candidate}"
            python_has_image_deps
            python_dependency_error
        )
        if(python_has_image_deps)
            set(${out_var} "${python_candidate}" PARENT_SCOPE)
            return()
        endif()
        if(first_dependency_error STREQUAL "")
            set(first_dependency_error "${python_dependency_error}")
        endif()
    endforeach()

    if(NOT BROOKESIA_GUI_LVGL_IMAGE_TOOL_AUTO_INSTALL_PIP_DEPS)
        message(FATAL_ERROR
            "LVGLImage.py requires the Python 'pypng', 'lz4', and 'Pillow' packages. "
            "Set BROOKESIA_GUI_LVGL_IMAGE_TOOL_PYTHON to a Python environment containing them, or enable "
            "BROOKESIA_GUI_LVGL_IMAGE_TOOL_AUTO_INSTALL_PIP_DEPS. Original error: ${first_dependency_error}"
        )
    endif()

    list(GET python_candidates 0 bootstrap_python)
    set(image_tool_venv "${CMAKE_BINARY_DIR}/brookesia_gui_lvgl_image_tool_venv")
    if(WIN32)
        set(image_tool_venv_python "${image_tool_venv}/Scripts/python.exe")
    else()
        set(image_tool_venv_python "${image_tool_venv}/bin/python")
    endif()

    if(NOT EXISTS "${image_tool_venv_python}")
        message(STATUS "Creating Brookesia GUI LVGL image tool Python environment: ${image_tool_venv}")
        execute_process(
            COMMAND "${bootstrap_python}" -m venv "${image_tool_venv}"
            RESULT_VARIABLE venv_result
            OUTPUT_VARIABLE venv_output
            ERROR_VARIABLE venv_error
        )
        if(NOT venv_result EQUAL 0)
            message(FATAL_ERROR
                "Failed to create Python virtual environment for LVGLImage.py with ${bootstrap_python}: "
                "${venv_output}${venv_error}"
            )
        endif()
    endif()

    message(STATUS "Installing Brookesia GUI LVGL image tool Python dependencies into ${image_tool_venv}")
    execute_process(
        COMMAND "${image_tool_venv_python}" -m pip install --disable-pip-version-check --quiet pypng lz4 Pillow
        RESULT_VARIABLE pip_result
        OUTPUT_VARIABLE pip_output
        ERROR_VARIABLE pip_error
    )
    if(NOT pip_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to install LVGLImage.py Python dependencies into ${image_tool_venv}: "
            "${pip_output}${pip_error}"
        )
    endif()

    brookesia_gui_lvgl_python_has_image_deps(
        "${image_tool_venv_python}"
        venv_has_image_deps
        venv_dependency_error
    )
    if(NOT venv_has_image_deps)
        message(FATAL_ERROR
            "Python environment still cannot import 'png', 'lz4.block', and 'PIL.Image' after installation: "
            "${venv_dependency_error}"
        )
    endif()

    set(${out_var} "${image_tool_venv_python}" PARENT_SCOPE)
endfunction()

function(brookesia_gui_lvgl_read_bin_size bin_path out_width out_height)
    file(READ "${bin_path}" bin_header HEX LIMIT 12)
    string(LENGTH "${bin_header}" bin_header_length)
    if(bin_header_length LESS 24)
        message(FATAL_ERROR "Invalid LVGL image bin header length: ${bin_path}")
    endif()

    string(SUBSTRING "${bin_header}" 0 2 bin_magic)
    if(NOT bin_magic STREQUAL "19")
        message(FATAL_ERROR "Invalid LVGL image bin magic '${bin_magic}': ${bin_path}")
    endif()

    string(SUBSTRING "${bin_header}" 8 2 width_byte0)
    string(SUBSTRING "${bin_header}" 10 2 width_byte1)
    string(SUBSTRING "${bin_header}" 12 2 height_byte0)
    string(SUBSTRING "${bin_header}" 14 2 height_byte1)
    math(EXPR bin_width "0x${width_byte1}${width_byte0}")
    math(EXPR bin_height "0x${height_byte1}${height_byte0}")

    if(bin_width LESS_EQUAL 0 OR bin_height LESS_EQUAL 0)
        message(FATAL_ERROR "Invalid LVGL image bin size ${bin_width}x${bin_height}: ${bin_path}")
    endif()

    set(${out_width} "${bin_width}" PARENT_SCOPE)
    set(${out_height} "${bin_height}" PARENT_SCOPE)
endfunction()

function(brookesia_gui_lvgl_pack_images)
    set(options PREMULTIPLY RGB565_DITHER)
    set(one_value_args INPUT_DIR OUTPUT_DIR CF COMPRESS ALIGN BACKGROUND)
    set(multi_value_args IMAGE_FILES)
    cmake_parse_arguments(pack "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT pack_INPUT_DIR)
        message(FATAL_ERROR "brookesia_gui_lvgl_pack_images requires INPUT_DIR")
    endif()
    if(NOT pack_OUTPUT_DIR)
        message(FATAL_ERROR "brookesia_gui_lvgl_pack_images requires OUTPUT_DIR")
    endif()

    if(NOT pack_CF)
        set(pack_CF RGB565A8)
    endif()
    if(NOT pack_COMPRESS)
        set(pack_COMPRESS NONE)
    endif()
    if(NOT pack_ALIGN)
        set(pack_ALIGN 1)
    endif()
    if(NOT pack_BACKGROUND)
        set(pack_BACKGROUND 0x000000)
    endif()

    get_filename_component(input_dir "${pack_INPUT_DIR}" ABSOLUTE)
    get_filename_component(output_dir "${pack_OUTPUT_DIR}" ABSOLUTE)
    if(NOT IS_DIRECTORY "${input_dir}")
        message(FATAL_ERROR "Image input directory does not exist: ${input_dir}")
    endif()

    brookesia_gui_lvgl_resolve_image_tool(image_tool)
    brookesia_gui_lvgl_resolve_image_tool_python(python_executable)

    if(pack_IMAGE_FILES)
        set(image_files)
        foreach(image_file IN LISTS pack_IMAGE_FILES)
            get_filename_component(image_file_absolute "${image_file}" ABSOLUTE)
            if(NOT EXISTS "${image_file_absolute}" OR IS_DIRECTORY "${image_file_absolute}")
                message(FATAL_ERROR "Image file does not exist: ${image_file_absolute}")
            endif()
            list(APPEND image_files "${image_file_absolute}")
        endforeach()
    else()
        # Do not use CONFIGURE_DEPENDS here. Some callers generate temporary input
        # directories during configure; registering those directories for Ninja
        # glob re-checks causes a configure/reconfigure loop.
        file(GLOB_RECURSE image_files
            "${input_dir}/*.png"
            "${input_dir}/*.PNG"
            "${input_dir}/*.jpg"
            "${input_dir}/*.JPG"
            "${input_dir}/*.jpeg"
            "${input_dir}/*.JPEG"
        )
    endif()
    list(SORT image_files)
    if(NOT image_files)
        message(FATAL_ERROR "No PNG/JPG/JPEG images found under ${input_dir}")
    endif()
    file(MAKE_DIRECTORY "${output_dir}")
    # Do not glob or delete existing JSON files from the output directory here.
    # Output directories are often configure-generated staging paths; tracking and
    # mutating them in the same configure pass makes Ninja re-run CMake forever.

    string(MD5 image_convert_cache_hash
        "${input_dir}_${output_dir}_${pack_CF}_${pack_COMPRESS}_${pack_ALIGN}_${pack_BACKGROUND}"
    )
    set(image_convert_cache_dir
        "${CMAKE_BINARY_DIR}/brookesia_gui_lvgl_image_pack_jpeg_${image_convert_cache_hash}"
    )
    file(REMOVE_RECURSE "${image_convert_cache_dir}")
    file(MAKE_DIRECTORY "${image_convert_cache_dir}")
    set(jpeg_to_png_script [=[
from PIL import Image, ImageOps
import sys

image = ImageOps.exif_transpose(Image.open(sys.argv[1]))
image.convert("RGBA").save(sys.argv[2])
]=])

    set(image_ids)
    set(image_descriptor_entries)
    foreach(image_file IN LISTS image_files)
        get_filename_component(image_id "${image_file}" NAME_WE)
        if(image_id IN_LIST image_ids)
            message(FATAL_ERROR "Duplicate image id '${image_id}' from image file names under ${input_dir}")
        endif()
        list(APPEND image_ids "${image_id}")

        get_filename_component(image_ext "${image_file}" EXT)
        string(TOLOWER "${image_ext}" image_ext_lower)
        set(image_tool_input "${image_file}")
        if(image_ext_lower STREQUAL ".jpg" OR image_ext_lower STREQUAL ".jpeg")
            set(converted_image_file "${image_convert_cache_dir}/${image_id}.png")
            execute_process(
                COMMAND "${python_executable}" -c "${jpeg_to_png_script}" "${image_file}" "${converted_image_file}"
                RESULT_VARIABLE jpeg_convert_result
                OUTPUT_VARIABLE jpeg_convert_output
                ERROR_VARIABLE jpeg_convert_error
            )
            if(NOT jpeg_convert_result EQUAL 0)
                message(FATAL_ERROR
                    "Failed to convert JPEG image '${image_file}' to temporary PNG: "
                    "${jpeg_convert_output}${jpeg_convert_error}"
                )
            endif()
            set(image_tool_input "${converted_image_file}")
        endif()

        set(image_tool_args
            "${image_tool}"
            --ofmt BIN
            --cf "${pack_CF}"
            --compress "${pack_COMPRESS}"
            --align "${pack_ALIGN}"
            --background "${pack_BACKGROUND}"
            -o "${output_dir}"
        )
        if(pack_PREMULTIPLY)
            list(APPEND image_tool_args --premultiply)
        endif()
        if(pack_RGB565_DITHER)
            list(APPEND image_tool_args --rgb565dither)
        endif()
        list(APPEND image_tool_args "${image_tool_input}")

        execute_process(
            COMMAND ${python_executable} ${image_tool_args}
            RESULT_VARIABLE convert_result
            OUTPUT_VARIABLE convert_output
            ERROR_VARIABLE convert_error
        )
        if(NOT convert_result EQUAL 0)
            message(FATAL_ERROR
                "Failed to convert image '${image_file}' with ${image_tool}: ${convert_output}${convert_error}"
            )
        endif()

        set(bin_path "${output_dir}/${image_id}.bin")
        if(NOT EXISTS "${bin_path}")
            message(FATAL_ERROR "LVGLImage.py did not generate expected bin file: ${bin_path}")
        endif()
        brookesia_gui_lvgl_read_bin_size("${bin_path}" image_width image_height)

        if(NOT "${image_descriptor_entries}" STREQUAL "")
            string(APPEND image_descriptor_entries ",\n")
        endif()
        string(APPEND image_descriptor_entries
            "        {\n"
            "            \"id\": \"${image_id}\",\n"
            "            \"src\": \"${image_id}.bin\",\n"
            "            \"width\": ${image_width},\n"
            "            \"height\": ${image_height}\n"
            "        }"
        )
    endforeach()

    file(WRITE "${output_dir}/index.json"
        "{\n"
        "    \"type\": \"imageSet\",\n"
        "    \"images\": [\n"
        "${image_descriptor_entries}\n"
        "    ]\n"
        "}\n"
    )

    message(STATUS
        "Brookesia GUI LVGL packed ${image_ids} image(s) from ${input_dir} using ${image_tool} with "
        "${python_executable}"
    )
endfunction()
