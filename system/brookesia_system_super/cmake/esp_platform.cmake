#
# ESP Platform
#
idf_component_register(
    SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    REQUIRES ${COMPONENT_REQUIRES}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)

if(DEFINED BROOKESIA_SYSTEM_CORE_COMPONENT_DIR AND NOT "${BROOKESIA_SYSTEM_CORE_COMPONENT_DIR}" STREQUAL "")
    set(brookesia_system_core_dir "${BROOKESIA_SYSTEM_CORE_COMPONENT_DIR}")
else()
    idf_build_get_property(build_components BUILD_COMPONENTS)
    if("espressif__brookesia_system_core" IN_LIST build_components)
        idf_component_get_property(brookesia_system_core_dir espressif__brookesia_system_core COMPONENT_DIR)
    else()
        idf_component_get_property(brookesia_system_core_dir brookesia_system_core COMPONENT_DIR)
    endif()
endif()
include("${brookesia_system_core_dir}/cmake/runtime_app_stage.cmake")
include(${COMPONENT_DIR}/cmake/resource_stage.cmake)
brookesia_system_core_get_esp_runtime_system_stage_root(system_super_system_stage_root)
if(CONFIG_BROOKESIA_SYSTEM_SUPER_ENABLE_RESOURCE_FONT_COPY)
    set(system_super_copy_fonts ON)
else()
    set(system_super_copy_fonts OFF)
endif()
brookesia_system_super_stage_resources(
    SYSTEM_STAGE_ROOT "${system_super_system_stage_root}"
    COPY_FONTS "${system_super_copy_fonts}"
)

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})
