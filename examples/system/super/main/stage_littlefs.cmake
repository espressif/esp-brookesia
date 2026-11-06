# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
#
# Build-time copy of the super littlefs tree, plus optional JSON asset bundles.
# Invoked by main/CMakeLists.txt after the app and system stage targets have
# written into SOURCE. cmake -P arguments: SOURCE, DEST, PYTHON, PYTHONPATH, PACK.

if(NOT SOURCE OR NOT DEST)
    message(FATAL_ERROR "stage_littlefs: SOURCE and DEST are required")
endif()

file(REMOVE_RECURSE "${DEST}")
file(MAKE_DIRECTORY "${DEST}")
file(COPY "${SOURCE}/" DESTINATION "${DEST}")

if(NOT PACK)
    return()
endif()
if(NOT PYTHON)
    message(FATAL_ERROR "stage_littlefs: PYTHON is required when PACK is set")
endif()

# JSON only: this tree already carries ~2 MB of images and the 5 MB littlefs
# partition has no room for a second copy. Products that want image bundles
# should size the partition first.
file(GLOB_RECURSE doc_roots "${DEST}/*/root.json")
foreach(doc_root ${doc_roots})
    get_filename_component(doc_dir "${doc_root}" DIRECTORY)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${PYTHONPATH}"
                "${PYTHON}" -m brookesia_gui_asset_bundle.cli pack
                --res-root "${doc_dir}" --out "${doc_dir}/assets.bundle"
                --include "*.json"
        RESULT_VARIABLE bundle_status
        OUTPUT_VARIABLE bundle_output
        ERROR_VARIABLE bundle_error
    )
    if(NOT bundle_status EQUAL 0)
        message(FATAL_ERROR "super: packing ${doc_dir} failed: ${bundle_error}")
    endif()
    if(bundle_output)
        message(STATUS "super: ${bundle_output}")
    endif()
endforeach()
