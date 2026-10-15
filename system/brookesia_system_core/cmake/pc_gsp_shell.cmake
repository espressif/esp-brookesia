# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

# The embedded bundles use the host assembler; enable it in directory scope.
enable_language(ASM)

# Compile the shared shell scenes, retaining both the native config and desktop API metadata.
function(brookesia_system_core_add_pc_gsp_shell target)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(PYTHON "${Python3_EXECUTABLE}")
    set(gui_gsp_dir "${COMPONENT_DIR}/../../gui/brookesia_gui_gsp")
    include("${gui_gsp_dir}/cmake/prepare_themes.cmake")
    set(scenes
        "${COMPONENT_DIR}/gsp/super_launcher_1024.json"
        "${COMPONENT_DIR}/gsp/super_keyboard_1024.json"
        "${COMPONENT_DIR}/gsp/super_dialog_1024.json"
    )
    if(NOT ESP_GSP_ROOT OR NOT EXISTS "${ESP_GSP_ROOT}/cmake/gsp_toolchain.cmake")
        message(FATAL_ERROR "GSP SystemCore requires ESP_GSP_ROOT pointing to esp-gsp")
    endif()
    foreach(theme IN ITEMS light dark)
        if(theme STREQUAL "light")
            set(symbol super_shell)
        else()
            set(symbol super_shell_dark)
        endif()
        brookesia_gsp_prepare_theme(prepared_scenes "${theme}" ${scenes})
        set(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/gsp_gen_${symbol}")
        set(bundle "${gen_dir}/${symbol}.gspb")
        set(api_header "${gen_dir}/${symbol}_gsp.h")
        set(api "${gen_dir}/${symbol}_gsp.api.json")
        set(depfile "${gen_dir}/${symbol}.d")
        set(embed "${gen_dir}/${symbol}_embed.S")
        file(MAKE_DIRECTORY "${gen_dir}")
        file(WRITE "${embed}"
            ".section .rodata\n.balign 64\n.global ${symbol}_start\n${symbol}_start:\n"
            ".incbin \"${bundle}\"\n.global ${symbol}_end\n${symbol}_end:\n"
            ".section .note.GNU-stack,\"\",@progbits\n")
        set(compiler_args "-DESP_GSP_ROOT=${ESP_GSP_ROOT}" "-DPROJECT_DIR=${PROJECT_SOURCE_DIR}")
        set(compiler_dependencies "")
        if(GSPC_EXECUTABLE)
            list(APPEND compiler_args "-DGSPC_EXECUTABLE=${GSPC_EXECUTABLE}")
            list(APPEND compiler_dependencies "${GSPC_EXECUTABLE}")
        endif()
        add_custom_command(
            OUTPUT "${bundle}" "${api_header}" "${api}"
            COMMAND "${CMAKE_COMMAND}" ${compiler_args}
                -P "${gui_gsp_dir}/cmake/gspc_build_step.cmake" --
                pack ${prepared_scenes} --pixel-format rgb565 --deployable
                --output "${bundle}" --gen-dir "${gen_dir}" --depfile "${depfile}"
                --api-header "${api_header}" --symbol "${symbol}"
            DEPENDS ${prepared_scenes} ${scenes} ${compiler_dependencies}
                "${gui_gsp_dir}/cmake/gspc_build_step.cmake"
            DEPFILE "${depfile}"
            VERBATIM
        )
        add_custom_target(${target}_${symbol} DEPENDS "${bundle}" "${api_header}" "${api}")
        add_dependencies(${target} ${target}_${symbol})
        set_source_files_properties("${embed}" PROPERTIES OBJECT_DEPENDS "${bundle}")
        target_sources(${target} PRIVATE "${embed}")
        target_include_directories(${target} PRIVATE "${gen_dir}")
        string(TOUPPER "${theme}" theme_upper)
        set_target_properties(${target} PROPERTIES
            BROOKESIA_GSP_SHELL_${theme_upper}_BUNDLE "${bundle}"
            BROOKESIA_GSP_SHELL_${theme_upper}_API "${api}"
        )
    endforeach()
endfunction()
