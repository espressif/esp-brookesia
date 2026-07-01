set(COMPONENT_LIB brookesia_service_http)
set(component_pc_config_compile_definitions "")

option(
    BROOKESIA_SERVICE_HTTP_ENABLE_AUTO_REGISTER
    "Default value of CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_AUTO_REGISTER on PC"
    ON
)
option(
    BROOKESIA_SERVICE_HTTP_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_DEBUG_LOG on PC"
    OFF
)
option(
    BROOKESIA_SERVICE_HTTP_PC_CONFIG_ENABLE_WORKER
    "Default value of CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_WORKER on PC"
    ON
)
option(
    BROOKESIA_SERVICE_HTTP_PC_CONFIG_ENABLE_HTTP
    "Default value of CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_HTTP on PC"
    ON
)
option(
    BROOKESIA_SERVICE_HTTP_PC_CONFIG_ENABLE_HTTPS
    "Default value of CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_HTTPS on PC"
    ON
)

set(_BROOKESIA_SERVICE_HTTP_SRCS_C ${COMPONENT_SRCS_C})
set(_BROOKESIA_SERVICE_HTTP_SRCS_CPP ${COMPONENT_SRCS_CPP})
# Some PC component CMake files inherit parent-scope component source variables.
# Clear them while adding dependencies, then restore the HTTP source list below.
set(COMPONENT_SRCS_C "")
set(COMPONENT_SRCS_CPP "")

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

if(NOT TARGET brookesia::service_helper)
    add_subdirectory(
        /../../framework/brookesia_service_helper
        ${CMAKE_BINARY_DIR}/brookesia_service_helper
    )
endif()

set(COMPONENT_SRCS_C ${_BROOKESIA_SERVICE_HTTP_SRCS_C})
set(COMPONENT_SRCS_CPP ${_BROOKESIA_SERVICE_HTTP_SRCS_CPP})

if(EMSCRIPTEN)
    set(BROOKESIA_SERVICE_HTTP_PC_CONFIG_ENABLE_WORKER OFF CACHE BOOL
        "Default value of CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_WORKER on PC" FORCE)
else()
    find_package(Boost REQUIRED COMPONENTS thread system chrono json)
endif()

if(BROOKESIA_SERVICE_HTTP_ENABLE_AUTO_REGISTER)
    set(BROOKESIA_SERVICE_HTTP_PLUGIN_SYMBOL service_http_symbol)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_AUTO_REGISTER=1)
    list(APPEND component_pc_config_compile_definitions
        BROOKESIA_SERVICE_HTTP_PLUGIN_SYMBOL=${BROOKESIA_SERVICE_HTTP_PLUGIN_SYMBOL}
    )
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_AUTO_REGISTER=0)
endif()

if(BROOKESIA_SERVICE_HTTP_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_DEBUG_LOG=1)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_HTTP_SERVICE_ENABLE_DEBUG_LOG=1)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_HTTP_STATE_MACHINE_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_SERVICE_HTTP_PC_CONFIG_ENABLE_WORKER)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_WORKER=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_WORKER=0)
endif()

if(BROOKESIA_SERVICE_HTTP_PC_CONFIG_ENABLE_HTTP)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_HTTP=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_HTTP=0)
endif()

if(BROOKESIA_SERVICE_HTTP_PC_CONFIG_ENABLE_HTTPS)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_HTTPS=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_HTTP_ENABLE_HTTPS=0)
endif()

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)
add_library(brookesia::service_http ALIAS ${COMPONENT_LIB})

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        BROOKESIA_SERVICE_HTTP_VER_MAJOR=${COMPONENT_VERSION_MAJOR}
        BROOKESIA_SERVICE_HTTP_VER_MINOR=${COMPONENT_VERSION_MINOR}
        BROOKESIA_SERVICE_HTTP_VER_PATCH=${COMPONENT_VERSION_PATCH}
        ${component_pc_config_compile_definitions}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::service_manager
        brookesia::service_helper
        brookesia::lib_utils
)
if(NOT EMSCRIPTEN)
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            Boost::thread
            Boost::system
            Boost::chrono
            Boost::json
    )
endif()

if(BROOKESIA_SERVICE_HTTP_ENABLE_AUTO_REGISTER)
    target_link_libraries(${COMPONENT_LIB} PUBLIC "-u ${BROOKESIA_SERVICE_HTTP_PLUGIN_SYMBOL}")
endif()

if(DEFINED BROOKESIA_SERVICE_HTTP_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_SERVICE_HTTP_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SERVICE_HTTP_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_SERVICE_HTTP_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_SERVICE_HTTP_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SERVICE_HTTP_PC_COMPILE_OPTIONS}
    )
endif()
