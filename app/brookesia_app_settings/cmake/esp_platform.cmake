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
target_link_libraries(${COMPONENT_LIB} PUBLIC "-u app_settings_provider_symbol")

set(app_settings_package_id "brookesia.general.settings")
idf_component_get_property(brookesia_system_core_dir brookesia_system_core COMPONENT_DIR)
include(${brookesia_system_core_dir}/cmake/runtime_app_stage.cmake)
brookesia_system_core_get_esp_runtime_app_stage_root(app_settings_stage_root)
brookesia_stage_runtime_app_package(
    PACKAGE_ID "${app_settings_package_id}"
    SOURCE_DIR "${COMPONENT_DIR}/package"
    STAGE_ROOT "${app_settings_stage_root}"
    NO_INDEX
)

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})
