#
# PC Platform
#
set(COMPONENT_LIB brookesia_app_files_impl)
option(
    BROOKESIA_APP_FILES_PC_CONFIG_ENABLE_PRELOAD_DOM
    "Default value of CONFIG_BROOKESIA_APP_FILES_ENABLE_PRELOAD_DOM on PC"
    ON
)

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)
brookesia_define_component_version(${COMPONENT_LIB} ${COMPONENT_DIR} BROOKESIA_APP_FILES)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::system_core
        brookesia::service_helper
        brookesia::service_storage
        "-u app_files_provider_symbol"
)
if(BROOKESIA_APP_FILES_PC_CONFIG_ENABLE_PRELOAD_DOM)
    set(BROOKESIA_APP_FILES_PC_PRELOAD_DOM_DEF CONFIG_BROOKESIA_APP_FILES_ENABLE_PRELOAD_DOM=1)
else()
    set(BROOKESIA_APP_FILES_PC_PRELOAD_DOM_DEF CONFIG_BROOKESIA_APP_FILES_ENABLE_PRELOAD_DOM=0)
endif()

target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        BROOKESIA_APP_FILES_PACKAGE_DIR="${COMPONENT_DIR}/package"
        ${BROOKESIA_APP_FILES_PC_PRELOAD_DOM_DEF}
)

set(app_files_package_id "brookesia.general.files")
include(${BROOKESIA_SYSTEM_CORE_COMPONENT_DIR}/cmake/runtime_app_stage.cmake)
if(NOT DEFINED BROOKESIA_SYSTEM_CORE_PC_STAGE_APP_ROOT)
    set(BROOKESIA_SYSTEM_CORE_PC_STAGE_APP_ROOT "${CMAKE_BINARY_DIR}/brookesia/apps")
endif()
brookesia_stage_runtime_app_package(
    PACKAGE_ID "${app_files_package_id}"
    SOURCE_DIR "${COMPONENT_DIR}/package"
    STAGE_ROOT "${BROOKESIA_SYSTEM_CORE_PC_STAGE_APP_ROOT}"
    NO_INDEX
)

if(DEFINED BROOKESIA_APP_FILES_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_APP_FILES_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_APP_FILES_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::app_files ALIAS ${COMPONENT_LIB})
