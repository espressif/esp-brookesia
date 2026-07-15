# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly

# Force Ninja to use response files to completely solve the 32KB Windows command line length limit
if("${CMAKE_GENERATOR}" MATCHES "Ninja")
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")
endif()

cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{RMAKER_PATH})
    set(RMAKER_PATH "$ENV{RMAKER_PATH}")
else()
    set(RMAKER_PATH "/home/miaojianyu/esp/Esp_Rainmaker/esp-rainmaker")
endif()

if(DEFINED ENV{ESP_MATTER_PATH})
    set(ESP_MATTER_PATH "$ENV{ESP_MATTER_PATH}")
else()
    set(ESP_MATTER_PATH "/home/miaojianyu/esp/ESP_MATTER/esp-matter_check/esp-matter")
    set(ENV{ESP_MATTER_PATH} "${ESP_MATTER_PATH}")
endif()

if(NOT EXISTS "${RMAKER_PATH}")
    message(FATAL_ERROR "ESP RainMaker repo not found. Set RMAKER_PATH before configuring examples/system/super.")
endif()

if(NOT EXISTS "${ESP_MATTER_PATH}")
    message(FATAL_ERROR "ESP Matter repo not found. Set ESP_MATTER_PATH before configuring examples/system/super.")
endif()

set(MATTER_SDK_PATH "${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip")
list(APPEND EXTRA_COMPONENT_DIRS
    "${MATTER_SDK_PATH}/config/esp32/components"
    "${CMAKE_CURRENT_LIST_DIR}/components/chip"
    "${ESP_MATTER_PATH}/components"
    "${ESP_MATTER_PATH}/examples/common"
    "${RMAKER_PATH}/examples/common/rmaker_matter_controller"
    "${RMAKER_PATH}/examples/common/rmaker_app_network"
    "${RMAKER_PATH}/examples/matter/common/app_controller"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Work around esp-idf-configdep mapping CONFIG_ESP_LVGL_ADAPTER_PARTIAL_AUX_IMG_CACHE
# to a Windows-invalid path containing the reserved device name "aux".
set(CONFIGDEP_ENABLE 0 CACHE BOOL "Disable esp-idf-configdep for Windows AUX path compatibility" FORCE)

project(example_system_super)

foreach(_rmaker_matter_comp app_controller rmaker_matter_controller)
    idf_component_get_property(_rmaker_matter_lib ${_rmaker_matter_comp} COMPONENT_LIB)
    if(_rmaker_matter_lib)
        target_link_libraries(${_rmaker_matter_lib} PUBLIC idf::esp_matter idf::esp_matter_controller)
    endif()
endforeach()

# ESP-Matter compiles a small set of ConnectedHomeIP sources directly through
# the IDF component build.  Those translation units must include the GN-
# generated CHIP build configuration exported by the `chip` component.
idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)

# Temporary workaround for `espressif__mcp-c-sdk` stringop-truncation warning
idf_build_set_property(COMPILE_OPTIONS -Wno-error=stringop-truncation APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=unused-result" APPEND)

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
