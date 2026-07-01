#
# PC Platform
#
set(COMPONENT_LIB brookesia_runtime_wasm_impl)
set(component_pc_config_compile_definitions "")

option(
    BROOKESIA_RUNTIME_WASM_ENABLE_AUTO_REGISTER
    "Default value of CONFIG_BROOKESIA_RUNTIME_WASM_ENABLE_AUTO_REGISTER on PC"
    ON
)
option(
    BROOKESIA_RUNTIME_WASM_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_RUNTIME_WASM_ENABLE_DEBUG_LOG on PC"
    OFF
)
option(
    BROOKESIA_RUNTIME_WASM_PC_CONFIG_BACKEND_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_RUNTIME_WASM_BACKEND_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_RUNTIME_WASM_PC_CONFIG_HOST_IMPORT_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_RUNTIME_WASM_HOST_IMPORT_ENABLE_DEBUG_LOG on PC"
    ON
)

find_path(WASMTIME_INCLUDE_DIR
    NAMES wasmtime.h wasm.h wasi.h
    HINTS $ENV{WASMTIME_C_API}
    PATH_SUFFIXES include
)
find_library(WASMTIME_LIBRARY
    NAMES wasmtime
    HINTS $ENV{WASMTIME_C_API}
    PATH_SUFFIXES lib
)

if(BROOKESIA_RUNTIME_WASM_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_WASM_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_WASM_ENABLE_DEBUG_LOG=0)
endif()
if(BROOKESIA_RUNTIME_WASM_ENABLE_AUTO_REGISTER)
    set(BROOKESIA_RUNTIME_WASM_PLUGIN_SYMBOL runtime_wasm_backend_symbol)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_WASM_ENABLE_AUTO_REGISTER=1)
    list(APPEND component_pc_config_compile_definitions BROOKESIA_RUNTIME_WASM_PLUGIN_SYMBOL=${BROOKESIA_RUNTIME_WASM_PLUGIN_SYMBOL})
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_WASM_ENABLE_AUTO_REGISTER=0)
endif()
if(BROOKESIA_RUNTIME_WASM_PC_CONFIG_BACKEND_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_WASM_BACKEND_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_WASM_BACKEND_ENABLE_DEBUG_LOG=0)
endif()
if(BROOKESIA_RUNTIME_WASM_PC_CONFIG_HOST_IMPORT_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_WASM_HOST_IMPORT_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_WASM_HOST_IMPORT_ENABLE_DEBUG_LOG=0)
endif()

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::runtime_manager
)
if(BROOKESIA_RUNTIME_WASM_ENABLE_AUTO_REGISTER)
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            "-u ${BROOKESIA_RUNTIME_WASM_PLUGIN_SYMBOL}"
    )
endif()
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        ${component_pc_config_compile_definitions}
)

if(WASMTIME_INCLUDE_DIR AND WASMTIME_LIBRARY)
    target_include_directories(${COMPONENT_LIB}
        SYSTEM PRIVATE
            ${WASMTIME_INCLUDE_DIR}
    )
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            ${WASMTIME_LIBRARY}
            pthread
            dl
            m
    )
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            BROOKESIA_RUNTIME_WASM_HAS_WASMTIME_C_API=1
    )
else()
    message(FATAL_ERROR
        "Wasmtime C API was not found; brookesia_runtime_wasm cannot provide host imports without it. "
        "Install the Wasmtime C API package or set WASMTIME_C_API to a directory containing "
        "include/wasmtime.h, include/wasm.h, include/wasi.h, and lib/libwasmtime.a or lib/libwasmtime.so."
    )
endif()

if(DEFINED BROOKESIA_RUNTIME_WASM_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_RUNTIME_WASM_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_RUNTIME_WASM_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_RUNTIME_WASM_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_RUNTIME_WASM_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_RUNTIME_WASM_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::runtime_wasm ALIAS ${COMPONENT_LIB})
