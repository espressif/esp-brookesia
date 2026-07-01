#
# PC Platform
#
set(COMPONENT_LIB brookesia_service_helper_impl)

if(NOT TARGET brookesia::hal_interface)
    add_subdirectory(
        ${COMPONENT_DIR}/../../hal/brookesia_hal_interface
        ${CMAKE_BINARY_DIR}/brookesia_hal_interface
    )
endif()

add_library(${COMPONENT_LIB} INTERFACE)
target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    INTERFACE
        ${COMPONENT_INCLUDE_DIRS}
)
target_link_libraries(${COMPONENT_LIB}
    INTERFACE
        brookesia::service_manager
        brookesia::hal_interface
        brookesia::lib_utils
        brookesia::hal_interface
)

if(DEFINED BROOKESIA_SERVICE_HELPER_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_SERVICE_HELPER_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        INTERFACE
            ${BROOKESIA_SERVICE_HELPER_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_SERVICE_HELPER_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_SERVICE_HELPER_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        INTERFACE
            ${BROOKESIA_SERVICE_HELPER_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::service_helper ALIAS ${COMPONENT_LIB})
