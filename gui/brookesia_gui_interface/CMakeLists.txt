set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_SRC_DIR ${COMPONENT_DIR}/src)
set(COMPONENT_EXAMPLES_DIR ${COMPONENT_SRC_DIR}/examples)
set(COMPONENT_INCLUDE_DIRS ${COMPONENT_DIR}/include)
set(COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_SRC_DIR})
set(COMPONENT_REQUIRES brookesia_lib_utils)
set(COMPONENT_SRCS_C "")
set(COMPONENT_SRCS_CPP "")
set(COMPONENT_SRCS_C_COMPILE_FLAGS "")
set(COMPONENT_SRCS_CPP_COMPILE_FLAGS "")

# Core Source Files (everything under src/ except the example suite in src/examples/).
file(GLOB_RECURSE GUI_INTERFACE_SRCS_C ${COMPONENT_SRC_DIR}/*.c)
file(GLOB_RECURSE GUI_INTERFACE_SRCS_CPP ${COMPONENT_SRC_DIR}/*.cpp)
list(FILTER GUI_INTERFACE_SRCS_C EXCLUDE REGEX "/src/examples/")
list(FILTER GUI_INTERFACE_SRCS_CPP EXCLUDE REGEX "/src/examples/")
list(APPEND COMPONENT_SRCS_C ${GUI_INTERFACE_SRCS_C})
list(APPEND COMPONENT_SRCS_CPP ${GUI_INTERFACE_SRCS_CPP})

# Example suite source files (built into a separate static library by the platform script).
file(GLOB_RECURSE GUI_INTERFACE_EXAMPLES_CPP ${COMPONENT_EXAMPLES_DIR}/*.cpp)

if(ESP_PLATFORM)
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/esp_platform.cmake)
else()
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/pc_platform.cmake)
endif()

#
# Compile Options
#
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
