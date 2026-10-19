# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly

# Force Ninja to use response files to completely solve the 32KB Windows command line length limit
if("${CMAKE_GENERATOR}" MATCHES "Ninja")
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")
endif()

cmake_minimum_required(VERSION 3.16)

option(CCACHE_ENABLE "Use ccache to accelerate repeat ESP-IDF builds" ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Work around esp-idf-configdep mapping CONFIG_ESP_LVGL_ADAPTER_PARTIAL_AUX_IMG_CACHE
# to a Windows-invalid path containing the reserved device name "aux".
# Only force this off on Windows: elsewhere it makes idf.py's default
# `-DCONFIGDEP_ENABLE=True` permanently disagree with the forced cache value, which
# makes idf.py re-run CMake configure on every build/flash (~24s each time).
if(CMAKE_HOST_WIN32)
    set(CONFIGDEP_ENABLE 0 CACHE BOOL "Disable esp-idf-configdep for Windows AUX path compatibility" FORCE)
endif()

project(example_system_super)

# Temporary workaround for `espressif__mcp-c-sdk` stringop-truncation warning
idf_build_set_property(COMPILE_OPTIONS -Wno-error=stringop-truncation APPEND)

brookesia_system_super_apply_compile_policy()

# Size-optimise P4's JSON UI and application orchestration within the existing
# factory partition. Pixel rendering, hardware services and media keep -O2.
# Keep this product policy here so it cannot change gsp_app's compile options.
if(IDF_TARGET STREQUAL "esp32p4")
    idf_build_get_property(super_components BUILD_COMPONENTS)
    foreach(super_size_component IN ITEMS
            brookesia_system_core
            brookesia_gui_interface
            brookesia_system_super
            brookesia_app_settings
            brookesia_app_store
            brookesia_app_files
            brookesia_service_manager)
        if(super_size_component IN_LIST super_components)
            set(super_component ${super_size_component})
        else()
            set(super_component espressif__${super_size_component})
        endif()
        idf_component_get_property(super_size_lib ${super_component} COMPONENT_LIB)
        target_compile_options(${super_size_lib} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Os>")
    endforeach()
endif()
