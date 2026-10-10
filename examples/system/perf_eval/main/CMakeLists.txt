idf_component_register(
    SRCS "perf_eval_main.cpp"
         "perf_bench.cpp"
         "perf_eval_res.c"
         "perf_eval_app.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_app_format
             esp_lvgl_adapter
             brookesia_service_manager
             brookesia_service_display
             brookesia_service_storage
             brookesia_gui_lvgl
             brookesia_system_core
)

# v1.8 section 2.3 Golden Case carrier: the JSON UI document tree is embedded in
# the firmware so the measurement does not depend on a mounted filesystem and a
# capture is reproducible from the binary alone.
#
# EMBED_TXTFILES only covers text, and this tree also carries .bin/.png assets,
# so every file is embedded through target_add_binary_data and perf_eval_res.c
# looks the bytes up by path.
# v1.8 section 2.3 Golden Case carrier: the JSON UI document tree is embedded in
# the firmware so the measurement needs no mounted filesystem and a capture is
# reproducible from the binary alone.
#
# ESP-IDF names target_add_binary_data symbols after the BASENAME only
# (_binary_<basename with . replaced by _>), not the path. In this tree
# flows/header.json and screens/header.json collide on _binary_header_json and
# one silently overwrites the other, so a document load would read the wrong file
# and still look healthy. Every file is therefore staged into a flat directory
# under a name derived from its full path, which is unique by construction.
# perf_eval_res.c maps the original paths onto those staged symbols.
set(PERF_EVAL_RES_DIR "${CMAKE_CURRENT_LIST_DIR}/../res")
set(PERF_EVAL_RES_STAGE "${CMAKE_CURRENT_BINARY_DIR}/res_stage")
file(REMOVE_RECURSE "${PERF_EVAL_RES_STAGE}")
file(MAKE_DIRECTORY "${PERF_EVAL_RES_STAGE}")
file(GLOB_RECURSE PERF_EVAL_RES_FILES "${PERF_EVAL_RES_DIR}/*")
foreach(res_file ${PERF_EVAL_RES_FILES})
    file(RELATIVE_PATH res_rel "${PERF_EVAL_RES_DIR}" "${res_file}")
    string(REPLACE "/" "_" res_flat "${res_rel}")
    string(REPLACE "." "_" res_flat "${res_flat}")
    # The font assets carry hyphens (system/fonts/NotoSansSC-Regular.subset.ttf),
    # which are not valid in a C identifier, so the symbol would not compile.
    string(REPLACE "-" "_" res_flat "${res_flat}")
    set(res_staged "${PERF_EVAL_RES_STAGE}/${res_flat}")
    configure_file("${res_file}" "${res_staged}" COPYONLY)
    target_add_binary_data(${COMPONENT_LIB} "${res_staged}" BINARY)
    # Give the staged file the basename the symbol is derived from.
    get_filename_component(res_base "${res_staged}" NAME)
    set_target_properties(${COMPONENT_LIB} PROPERTIES
        LINK_DEPENDS "${res_staged}")
endforeach()
message(STATUS "perf_eval: staged ${CMAKE_CURRENT_BINARY_DIR}/res_stage")

# v1.8 section 5.1: sdkconfig_hash must identify the build configuration so a
# baseline and an optimized run from the same config share it. Hash the resolved
# sdkconfig; the previous git-sha stand-in changed whenever the optimization was
# committed and blocked every cross-commit paired comparison.
if(EXISTS "${CMAKE_SOURCE_DIR}/sdkconfig")
    file(SHA256 "${CMAKE_SOURCE_DIR}/sdkconfig" PERF_EVAL_SDKCONFIG_SHA)
else()
    set(PERF_EVAL_SDKCONFIG_SHA "unavailable")
endif()
# A generated header is used instead of a quoted compile definition: CMake strips
# the inner quotes of -DMACRO="..." so the value landed in the metadata JSON
# without its closing quote, producing an unparseable record.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/perf_eval_build_info.h"
    "#pragma once\n#define PERF_EVAL_SDKCONFIG_HASH \"sha256:${PERF_EVAL_SDKCONFIG_SHA}\"\n")
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
