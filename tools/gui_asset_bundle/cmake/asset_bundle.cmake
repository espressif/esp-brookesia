# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
#
# Reusable packaging step for tools/gui_asset_bundle.
#
# brookesia_gui_pack_assets(<target> RES_DIR <dir> OUT <file> [PATTERNS <glob>...])
#   Adds a custom target that produces <file> from every matching file under
#   <dir> and makes <target> depend on it. The command depends on the whole
#   resource tree, so editing an asset re-runs the packer instead of leaving a
#   stale bundle behind.
#
# Typical use is to generate <res>/assets.bundle before the resource tree is
# turned into a littlefs image, so the runtime can open one file instead of one
# per asset. Hosts that need the file at configure time (for example to embed it
# with target_add_binary_data) call the CLI directly instead:
#
#   python -m brookesia_gui_asset_bundle.cli pack --res-root <dir> --out <file>
#
# The packer is deterministic: identical inputs produce identical bytes, which is
# what makes a CI staleness check possible.

set(BROOKESIA_GUI_ASSET_BUNDLE_SRC_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../src"
    CACHE INTERNAL "Python sources of the GUI asset bundle tool")

function(brookesia_gui_pack_assets target)
    cmake_parse_arguments(ARG "" "RES_DIR;OUT" "PATTERNS" ${ARGN})
    if(NOT ARG_RES_DIR OR NOT ARG_OUT)
        message(FATAL_ERROR "brookesia_gui_pack_assets: RES_DIR and OUT are required")
    endif()
    if(NOT TARGET ${target})
        message(FATAL_ERROR "brookesia_gui_pack_assets: unknown target '${target}'")
    endif()

    if(NOT PYTHON)
        set(PYTHON python3)
    endif()

    set(patterns_args "")
    foreach(pattern ${ARG_PATTERNS})
        list(APPEND patterns_args --include "${pattern}")
    endforeach()

    get_filename_component(out_dir "${ARG_OUT}" DIRECTORY)
    file(GLOB_RECURSE res_files "${ARG_RES_DIR}/*")

    add_custom_command(
        OUTPUT "${ARG_OUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${out_dir}"
        COMMAND ${CMAKE_COMMAND} -E env
                "PYTHONPATH=${BROOKESIA_GUI_ASSET_BUNDLE_SRC_DIR}"
                "${PYTHON}" -m brookesia_gui_asset_bundle.cli pack
                --res-root "${ARG_RES_DIR}" --out "${ARG_OUT}" ${patterns_args}
        DEPENDS ${res_files}
        COMMENT "Packing GUI assets from ${ARG_RES_DIR} into ${ARG_OUT}"
        VERBATIM
    )
    add_custom_target(${target}_asset_bundle DEPENDS "${ARG_OUT}")
    add_dependencies(${target} ${target}_asset_bundle)
endfunction()
