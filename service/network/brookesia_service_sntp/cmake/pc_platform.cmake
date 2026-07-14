#
# PC Platform
#
option(
    BROOKESIA_SERVICE_SNTP_ENABLE_AUTO_REGISTER
    "Default value of CONFIG_BROOKESIA_SERVICE_SNTP_ENABLE_AUTO_REGISTER on PC"
    ON
)
option(
    BROOKESIA_SERVICE_SNTP_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SERVICE_SNTP_ENABLE_DEBUG_LOG on PC"
    OFF
)

if(NOT TARGET brookesia::lib_utils)
    add_subdirectory(
        ${COMPONENT_DIR}/../../utils/brookesia_lib_utils
        ${CMAKE_BINARY_DIR}/brookesia_lib_utils
    )
endif()
if(NOT TARGET brookesia::service_manager)
    add_subdirectory(
        /../../framework/brookesia_service_manager
        ${CMAKE_BINARY_DIR}/brookesia_service_manager
    )
endif()
if(NOT TARGET brookesia::hal_interface)
    add_subdirectory(
        ${COMPONENT_DIR}/../../hal/brookesia_hal_interface
        ${CMAKE_BINARY_DIR}/brookesia_hal_interface
    )
endif()
if(NOT TARGET brookesia::service_helper)
    add_subdirectory(
        /../../framework/brookesia_service_helper
        ${CMAKE_BINARY_DIR}/brookesia_service_helper
    )
endif()
if(NOT TARGET brookesia::service_storage)
    add_subdirectory(
        /../../system/brookesia_service_storage
        ${CMAKE_BINARY_DIR}/brookesia_service_storage
    )
endif()

find_package(Boost REQUIRED COMPONENTS thread system chrono json)

set(component_pc_config_compile_definitions)
if(BROOKESIA_SERVICE_SNTP_ENABLE_AUTO_REGISTER)
    set(BROOKESIA_SERVICE_SNTP_PLUGIN_SYMBOL service_sntp_symbol)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_SNTP_ENABLE_AUTO_REGISTER=1)
    list(APPEND component_pc_config_compile_definitions
        BROOKESIA_SERVICE_SNTP_PLUGIN_SYMBOL=${BROOKESIA_SERVICE_SNTP_PLUGIN_SYMBOL}
    )
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_SNTP_ENABLE_AUTO_REGISTER=0)
endif()
if(BROOKESIA_SERVICE_SNTP_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_SNTP_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_SNTP_ENABLE_DEBUG_LOG=0)
endif()

set(COMPONENT_LIB brookesia_service_sntp)
add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)
brookesia_define_component_version(${COMPONENT_LIB} ${COMPONENT_DIR} BROOKESIA_SERVICE_SNTP)
add_library(brookesia::service_sntp ALIAS ${COMPONENT_LIB})

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::service_manager
        brookesia::service_helper
        brookesia::service_storage
        brookesia::hal_interface
        brookesia::lib_utils
        Boost::thread
        Boost::system
        Boost::chrono
        Boost::json
)
if(BROOKESIA_SERVICE_SNTP_ENABLE_AUTO_REGISTER)
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            "-u ${BROOKESIA_SERVICE_SNTP_PLUGIN_SYMBOL}"
    )
endif()
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        ${component_pc_config_compile_definitions}
)
