idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE TRUE
)

# Derive the expected service version independently from the component manifest.
idf_build_get_property(build_components BUILD_COMPONENTS)
if("espressif__brookesia_lib_utils" IN_LIST build_components)
    idf_component_get_property(lib_utils_dir espressif__brookesia_lib_utils COMPONENT_DIR)
else()
    idf_component_get_property(lib_utils_dir brookesia_lib_utils COMPONENT_DIR)
endif()
include("${lib_utils_dir}/cmake/component_version.cmake")
brookesia_parse_component_version("${CMAKE_CURRENT_LIST_DIR}/../.." test_component_version)
target_compile_definitions(${COMPONENT_LIB} PRIVATE BROOKESIA_TEST_COMPONENT_VERSION="${test_component_version}")
