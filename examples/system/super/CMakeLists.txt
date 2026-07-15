# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly

# Force Ninja to use response files to completely solve the 32KB Windows command line length limit
if("${CMAKE_GENERATOR}" MATCHES "Ninja")
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")
endif()

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Work around esp-idf-configdep mapping CONFIG_ESP_LVGL_ADAPTER_PARTIAL_AUX_IMG_CACHE
# to a Windows-invalid path containing the reserved device name "aux".
set(CONFIGDEP_ENABLE 0 CACHE BOOL "Disable esp-idf-configdep for Windows AUX path compatibility" FORCE)

project(example_system_super)

# Temporary workaround for `espressif__mcp-c-sdk` stringop-truncation warning
idf_build_set_property(COMPILE_OPTIONS -Wno-error=stringop-truncation APPEND)

# [EXPERIMENT] Fold per-TU std::string IPA clones in brookesia components.
# Disables GCC interprocedural clone passes (does NOT lower the -O level) so the
# many .isra/.constprop std::string helper clones collapse to single shared copies.
idf_build_get_property(_brookesia_build_components BUILD_COMPONENTS)
foreach(_brookesia_comp ${_brookesia_build_components})
    if(_brookesia_comp MATCHES "brookesia")
        set(_brookesia_lib "__idf_${_brookesia_comp}")
        if(TARGET ${_brookesia_lib})
            get_target_property(_brookesia_lib_type ${_brookesia_lib} TYPE)
            if(NOT _brookesia_lib_type STREQUAL "INTERFACE_LIBRARY")
                target_compile_options(${_brookesia_lib} PRIVATE -fno-ipa-cp-clone -fno-ipa-sra)
            endif()
        endif()
    endif()
endforeach()
