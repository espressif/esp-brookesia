cmake_minimum_required(VERSION 3.16)

# Stage into this build directory so a GSP configure and an LVGL configure do not
# keep writing into the same source-tree littlefs folder. An explicit non-empty
# -D BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT=... still wins.
if(NOT DEFINED BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT OR
        "${BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT}" STREQUAL "")
    set(BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT "${CMAKE_BINARY_DIR}/littlefs_stage"
        CACHE PATH "Host directory packed into the littlefs image" FORCE)
    set(brookesia_runtime_path_explicit_cache BROOKESIA_SYSTEM_CORE_ESP_RUNTIME_PATHS_EXPLICIT_CACHE_VARS)
    get_property(brookesia_explicit_paths_set CACHE ${brookesia_runtime_path_explicit_cache} PROPERTY VALUE SET)
    if(brookesia_explicit_paths_set)
        get_property(brookesia_explicit_paths CACHE ${brookesia_runtime_path_explicit_cache} PROPERTY VALUE)
    else()
        set(brookesia_explicit_paths)
    endif()
    list(APPEND brookesia_explicit_paths BROOKESIA_SYSTEM_CORE_ESP_RESOURCE_STAGE_ROOT)
    list(REMOVE_DUPLICATES brookesia_explicit_paths)
    set(${brookesia_runtime_path_explicit_cache} "${brookesia_explicit_paths}"
        CACHE INTERNAL "ESP runtime path cache variables explicitly set by the project" FORCE)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(example_system_gsp_app)

brookesia_compile_tuning_apply_current_project()
