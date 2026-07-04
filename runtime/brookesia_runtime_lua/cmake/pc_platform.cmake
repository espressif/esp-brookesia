#
# PC Platform
#
set(COMPONENT_LIB brookesia_runtime_lua_impl)
set(component_pc_config_compile_definitions "")

option(
    BROOKESIA_RUNTIME_LUA_ENABLE_AUTO_REGISTER
    "Default value of CONFIG_BROOKESIA_RUNTIME_LUA_ENABLE_AUTO_REGISTER on PC"
    ON
)
option(
    BROOKESIA_RUNTIME_LUA_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_RUNTIME_LUA_ENABLE_DEBUG_LOG on PC"
    OFF
)
option(
    BROOKESIA_RUNTIME_LUA_PC_CONFIG_BACKEND_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_RUNTIME_LUA_BACKEND_ENABLE_DEBUG_LOG on PC"
    ON
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LUA54 REQUIRED lua5.4)

set(lua54_include_dirs ${LUA54_INCLUDE_DIRS})
set(lua54_library_dirs ${LUA54_LIBRARY_DIRS})
set(lua54_libraries ${LUA54_LIBRARIES})
set(lua54_cflags_other ${LUA54_CFLAGS_OTHER})
set(lua54_link_options ${LUA54_LDFLAGS_OTHER})

if(BROOKESIA_SIMULATOR_SYSTEM_FORCE_STATIC_EXECUTABLE)
    set(lua54_library_dirs ${LUA54_STATIC_LIBRARY_DIRS})
    set(lua54_libraries ${LUA54_STATIC_LIBRARIES})
    set(lua54_link_options ${LUA54_STATIC_LDFLAGS_OTHER})
endif()

if(BROOKESIA_RUNTIME_LUA_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_LUA_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_LUA_ENABLE_DEBUG_LOG=0)
endif()
if(BROOKESIA_RUNTIME_LUA_ENABLE_AUTO_REGISTER)
    set(BROOKESIA_RUNTIME_LUA_PLUGIN_SYMBOL runtime_lua_backend_symbol)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_LUA_ENABLE_AUTO_REGISTER=1)
    list(APPEND component_pc_config_compile_definitions BROOKESIA_RUNTIME_LUA_PLUGIN_SYMBOL=${BROOKESIA_RUNTIME_LUA_PLUGIN_SYMBOL})
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_LUA_ENABLE_AUTO_REGISTER=0)
endif()
if(BROOKESIA_RUNTIME_LUA_PC_CONFIG_BACKEND_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_LUA_BACKEND_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_LUA_BACKEND_ENABLE_DEBUG_LOG=0)
endif()

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
        ${lua54_include_dirs}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_link_directories(${COMPONENT_LIB}
    PUBLIC
        ${lua54_library_dirs}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::runtime_manager
        ${lua54_libraries}
)
if(BROOKESIA_RUNTIME_LUA_ENABLE_AUTO_REGISTER)
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            "-u ${BROOKESIA_RUNTIME_LUA_PLUGIN_SYMBOL}"
    )
endif()
target_compile_options(${COMPONENT_LIB}
    PRIVATE
        ${lua54_cflags_other}
)
if(lua54_link_options)
    target_link_options(${COMPONENT_LIB}
        PUBLIC
            ${lua54_link_options}
    )
endif()
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        ${component_pc_config_compile_definitions}
)

if(DEFINED BROOKESIA_RUNTIME_LUA_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_RUNTIME_LUA_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_RUNTIME_LUA_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_RUNTIME_LUA_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_RUNTIME_LUA_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_RUNTIME_LUA_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::runtime_lua ALIAS ${COMPONENT_LIB})
