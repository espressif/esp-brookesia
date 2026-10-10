# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Optional ESP-IDF component that preinstalls Device Control in the firmware's
# LittleFS image. The package is built with tools/build_package.py, the same
# script used for USB installs.
idf_component_register()

set(device_control_package_id "brookesia.gsp.device_control")
file(READ "${COMPONENT_DIR}/package/manifest.json" device_control_manifest)
string(JSON device_control_version GET "${device_control_manifest}" package version)

idf_build_get_property(device_control_build_components BUILD_COMPONENTS)
if("espressif__brookesia_system_core" IN_LIST device_control_build_components)
    idf_component_get_property(brookesia_system_core_dir espressif__brookesia_system_core COMPONENT_DIR)
else()
    idf_component_get_property(brookesia_system_core_dir brookesia_system_core COMPONENT_DIR)
endif()
include("${brookesia_system_core_dir}/cmake/runtime_app_stage.cmake")
brookesia_system_core_get_esp_runtime_app_stage_root(device_control_stage_root)
brookesia_system_core_cleanup_runtime_stage_root("${device_control_stage_root}")

set(device_control_build_dir "${CMAKE_CURRENT_BINARY_DIR}/package_build")
set(device_control_stage_dir "${device_control_stage_root}/${device_control_package_id}")
set(device_control_stamp "${CMAKE_CURRENT_BINARY_DIR}/device_control_built.stamp")

set(device_control_gspc_args "")
if(DEFINED GSPC_EXECUTABLE AND NOT "${GSPC_EXECUTABLE}" STREQUAL "")
    set(device_control_gspc_args --gspc "${GSPC_EXECUTABLE}")
elseif(DEFINED ENV{GSPC_EXECUTABLE} AND NOT "$ENV{GSPC_EXECUTABLE}" STREQUAL "")
    set(device_control_gspc_args --gspc "$ENV{GSPC_EXECUTABLE}")
endif()

file(GLOB_RECURSE device_control_sources CONFIGURE_DEPENDS
    "${COMPONENT_DIR}/package/*"
    "${COMPONENT_DIR}/scenes/*.json"
)
add_custom_command(
    OUTPUT "${device_control_stamp}"
    COMMAND "${PYTHON}" "${COMPONENT_DIR}/tools/build_package.py"
        --version "${device_control_version}"
        --output "${device_control_build_dir}"
        ${device_control_gspc_args}
    COMMAND ${CMAKE_COMMAND} -E touch "${device_control_stamp}"
    DEPENDS ${device_control_sources} "${COMPONENT_DIR}/tools/build_package.py"
    COMMENT "Building ${device_control_package_id} ${device_control_version}"
    VERBATIM
)
# Every reconfigure empties the stage root, so copy on every build rather than once per package build.
add_custom_target(brookesia_stage_device_control ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${device_control_stage_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${device_control_build_dir}/staging" "${device_control_stage_dir}"
    DEPENDS "${device_control_stamp}"
    COMMENT "Staging ${device_control_package_id} ${device_control_version}"
    VERBATIM
)

get_property(device_control_stage_targets GLOBAL PROPERTY BROOKESIA_RUNTIME_APP_STAGE_TARGETS)
list(APPEND device_control_stage_targets brookesia_stage_device_control)
list(REMOVE_DUPLICATES device_control_stage_targets)
set_property(GLOBAL PROPERTY BROOKESIA_RUNTIME_APP_STAGE_TARGETS "${device_control_stage_targets}")
