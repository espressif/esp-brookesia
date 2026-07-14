#
# ESP Platform
#
idf_component_register(
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
)

include(package_manager)
include(${COMPONENT_DIR}/../../../utils/brookesia_lib_utils/cmake/component_version.cmake)
brookesia_define_component_version(${COMPONENT_LIB} ${COMPONENT_DIR} BROOKESIA_SERVICE_HELPER)
