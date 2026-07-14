#
# PC Platform
#
set(COMPONENT_LIB brookesia_service_manager_impl)
set(component_pc_config_compile_definitions "")

if(NOT EMSCRIPTEN)
    find_package(Boost REQUIRED COMPONENTS thread chrono json)
endif()

option(
    BROOKESIA_SERVICE_MANAGER_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SERVICE_MANAGER_ENABLE_DEBUG_LOG on PC"
    OFF
)
option(
    BROOKESIA_SERVICE_MANAGER_PC_CONFIG_EVENT_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SERVICE_MANAGER_EVENT_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_SERVICE_MANAGER_PC_CONFIG_FUNCTION_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SERVICE_MANAGER_FUNCTION_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_SERVICE_MANAGER_PC_CONFIG_SERVICE_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SERVICE_MANAGER_SERVICE_ENABLE_DEBUG_LOG on PC"
    ON
)

if(BROOKESIA_SERVICE_MANAGER_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_MANAGER_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_MANAGER_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_SERVICE_MANAGER_PC_CONFIG_EVENT_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_MANAGER_EVENT_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_MANAGER_EVENT_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_SERVICE_MANAGER_PC_CONFIG_FUNCTION_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_MANAGER_FUNCTION_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_MANAGER_FUNCTION_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_SERVICE_MANAGER_PC_CONFIG_SERVICE_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_MANAGER_SERVICE_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_MANAGER_SERVICE_ENABLE_DEBUG_LOG=0)
endif()

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)
brookesia_define_component_version(${COMPONENT_LIB} ${COMPONENT_DIR} BROOKESIA_SERVICE_MANAGER)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
if(Boost_INCLUDE_DIRS)
    target_include_directories(${COMPONENT_LIB} PUBLIC ${Boost_INCLUDE_DIRS})
endif()

if(EMSCRIPTEN)
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            brookesia::lib_utils
    )
else()
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            brookesia::lib_utils
            Boost::thread
            Boost::chrono
            Boost::json
    )
endif()

target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        ${component_pc_config_compile_definitions}
)

if(DEFINED BROOKESIA_SERVICE_MANAGER_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_SERVICE_MANAGER_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SERVICE_MANAGER_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_SERVICE_MANAGER_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_SERVICE_MANAGER_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SERVICE_MANAGER_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::service_manager ALIAS ${COMPONENT_LIB})
