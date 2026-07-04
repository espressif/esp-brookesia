set(COMPONENT_DIR ${CMAKE_CURRENT_LIST_DIR})
set(COMPONENT_SRC_DIR ${COMPONENT_DIR}/src)
set(COMPONENT_INCLUDE_DIRS ${COMPONENT_DIR}/include)
set(COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_SRC_DIR})
set(COMPONENT_REQUIRES "")
set(COMPONENT_SRCS_C "")
set(COMPONENT_SRCS_CPP "")
set(COMPONENT_SRCS_C_COMPILE_FLAGS "")
set(COMPONENT_SRCS_CPP_COMPILE_FLAGS "")

file(
    STRINGS "${COMPONENT_DIR}/idf_component.yml" component_version_line
    REGEX "^[ \t]*version:[ \t]*\"?[0-9]+\\.[0-9]+\\.[0-9]+\"?[ \t]*$"
)
if(NOT component_version_line)
    message(FATAL_ERROR "Failed to parse brookesia_system_super version from idf_component.yml")
endif()
list(GET component_version_line 0 component_version_line)
string(
    REGEX REPLACE
    "^[ \t]*version:[ \t]*\"?([0-9]+)\\.([0-9]+)\\.([0-9]+)\"?[ \t]*$"
    "\\1;\\2;\\3"
    component_version_parts
    "${component_version_line}"
)
list(GET component_version_parts 0 COMPONENT_VERSION_MAJOR)
list(GET component_version_parts 1 COMPONENT_VERSION_MINOR)
list(GET component_version_parts 2 COMPONENT_VERSION_PATCH)

file(GLOB_RECURSE SYSTEM_SUPER_SRCS_C ${COMPONENT_SRC_DIR}/*.c)
file(GLOB_RECURSE SYSTEM_SUPER_SRCS_CPP ${COMPONENT_SRC_DIR}/*.cpp)
list(APPEND COMPONENT_SRCS_C ${SYSTEM_SUPER_SRCS_C})
list(APPEND COMPONENT_SRCS_CPP ${SYSTEM_SUPER_SRCS_CPP})

if(ESP_PLATFORM)
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/esp_platform.cmake)
else()
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/pc_platform.cmake)
endif()

if(NOT CMAKE_SCRIPT_MODE_FILE)
    set_source_files_properties(${COMPONENT_SRCS_C}
        PROPERTIES
            COMPILE_FLAGS "${COMPONENT_SRCS_C_COMPILE_FLAGS}"
    )
    set_source_files_properties(${COMPONENT_SRCS_CPP}
        PROPERTIES
            COMPILE_FLAGS "${COMPONENT_SRCS_CPP_COMPILE_FLAGS}"
    )
endif()
