#
# PC Platform
#
set(COMPONENT_LIB brookesia_runtime_js_impl)
set(component_pc_config_compile_definitions "")

option(
    BROOKESIA_RUNTIME_JS_ENABLE_AUTO_REGISTER
    "Default value of CONFIG_BROOKESIA_RUNTIME_JS_ENABLE_AUTO_REGISTER on PC"
    ON
)
option(
    BROOKESIA_RUNTIME_JS_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_RUNTIME_JS_ENABLE_DEBUG_LOG on PC"
    OFF
)
option(
    BROOKESIA_RUNTIME_JS_PC_CONFIG_BACKEND_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_RUNTIME_JS_BACKEND_ENABLE_DEBUG_LOG on PC"
    ON
)

# QUICKJS_ROOT: system/source prefix (quickjs.h + libquickjs.a) or staged prefix
# (include/quickjs.h + lib/libquickjs.a).
# Optional QUICKJS_BUILD_DIR: directory containing libqjs.a (defaults to $QUICKJS_ROOT/build-pc).
set(QUICKJS_ROOT "$ENV{QUICKJS_ROOT}" CACHE PATH "QuickJS installation or source root")
set(QUICKJS_BUILD_DIR "$ENV{QUICKJS_BUILD_DIR}" CACHE PATH "QuickJS build directory")
set(brookesia_runtime_js_default_quickjs_ng_dir "")
if(DEFINED BROOKESIA_SIMULATOR_QUICKJS_NG_DIR AND NOT "${BROOKESIA_SIMULATOR_QUICKJS_NG_DIR}" STREQUAL "")
    set(brookesia_runtime_js_default_quickjs_ng_dir "${BROOKESIA_SIMULATOR_QUICKJS_NG_DIR}")
endif()
set(
    BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR
    "${brookesia_runtime_js_default_quickjs_ng_dir}"
    CACHE PATH
    "QuickJS-NG source directory used by the wasm JavaScript runtime"
)
option(
    BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_AUTO_FETCH
    "Fetch QuickJS-NG source automatically for wasm JavaScript runtime"
    ON
)
if(DEFINED BROOKESIA_SIMULATOR_QUICKJS_NG_AUTO_FETCH)
    set(
        BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_AUTO_FETCH
        "${BROOKESIA_SIMULATOR_QUICKJS_NG_AUTO_FETCH}"
        CACHE BOOL
        "Fetch QuickJS-NG source automatically for wasm JavaScript runtime"
        FORCE
    )
endif()

if(NOT EMSCRIPTEN)
    find_program(QUICKJS_QJS_EXECUTABLE
        NAMES qjs quickjs quickjs-ng
        HINTS ${QUICKJS_ROOT}
        PATH_SUFFIXES bin .
    )
    if(NOT QUICKJS_ROOT AND QUICKJS_QJS_EXECUTABLE)
        get_filename_component(quickjs_qjs_dir "${QUICKJS_QJS_EXECUTABLE}" DIRECTORY)
        get_filename_component(quickjs_qjs_parent_dir "${quickjs_qjs_dir}" DIRECTORY)
        if(EXISTS "${quickjs_qjs_dir}/quickjs.h")
            set(QUICKJS_ROOT "${quickjs_qjs_dir}" CACHE PATH "QuickJS installation or source root" FORCE)
        elseif(EXISTS "${quickjs_qjs_parent_dir}/include/quickjs.h")
            set(QUICKJS_ROOT "${quickjs_qjs_parent_dir}" CACHE PATH "QuickJS installation or source root" FORCE)
        endif()
    endif()

    find_path(QUICKJS_INCLUDE_DIR
        NAMES quickjs.h
        HINTS ${QUICKJS_ROOT}
        PATH_SUFFIXES . include include/quickjs
    )
    set(quickjs_build_dir "${QUICKJS_BUILD_DIR}")
    if(quickjs_build_dir STREQUAL "" AND QUICKJS_ROOT)
        set(quickjs_build_dir "${QUICKJS_ROOT}/build-pc")
    endif()
    find_library(QUICKJS_LIBRARY
        NAMES quickjs quickjs-ng qjs
        HINTS
            ${QUICKJS_ROOT}
            ${quickjs_build_dir}
        PATH_SUFFIXES lib lib/quickjs .
    )
    find_library(QUICKJS_LIBC_LIBRARY
        NAMES qjs-libc quickjs-libc
        HINTS
            ${QUICKJS_ROOT}
            ${quickjs_build_dir}
        PATH_SUFFIXES lib lib/quickjs .
    )
endif()

if(BROOKESIA_RUNTIME_JS_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_JS_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_JS_ENABLE_DEBUG_LOG=0)
endif()
if(BROOKESIA_RUNTIME_JS_ENABLE_AUTO_REGISTER)
    set(BROOKESIA_RUNTIME_JS_PLUGIN_SYMBOL runtime_js_backend_symbol)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_JS_ENABLE_AUTO_REGISTER=1)
    list(APPEND component_pc_config_compile_definitions BROOKESIA_RUNTIME_JS_PLUGIN_SYMBOL=${BROOKESIA_RUNTIME_JS_PLUGIN_SYMBOL})
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_JS_ENABLE_AUTO_REGISTER=0)
endif()
if(BROOKESIA_RUNTIME_JS_PC_CONFIG_BACKEND_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_JS_BACKEND_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_RUNTIME_JS_BACKEND_ENABLE_DEBUG_LOG=0)
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
if(BROOKESIA_RUNTIME_JS_ENABLE_AUTO_REGISTER)
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            "-u ${BROOKESIA_RUNTIME_JS_PLUGIN_SYMBOL}"
    )
endif()
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        ${component_pc_config_compile_definitions}
)

if(EMSCRIPTEN)
    if(NOT TARGET brookesia_quickjs_ng_wasm)
        set(brookesia_runtime_js_quickjs_ng_source_valid OFF)
        if(EXISTS "${BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR}/quickjs.c")
            if(EXISTS "${BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR}/VERSION")
                file(READ "${BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR}/VERSION"
                    brookesia_runtime_js_quickjs_ng_version)
                string(STRIP "${brookesia_runtime_js_quickjs_ng_version}"
                    brookesia_runtime_js_quickjs_ng_version)
                if(brookesia_runtime_js_quickjs_ng_version MATCHES "^0\\.14\\.")
                    set(brookesia_runtime_js_quickjs_ng_source_valid ON)
                endif()
            endif()
        endif()
        if(NOT brookesia_runtime_js_quickjs_ng_source_valid)
            if(BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_AUTO_FETCH)
                include(FetchContent)
                if(BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR)
                    message(STATUS
                        "QuickJS-NG 0.14 source was not found at ${BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR}; "
                        "fetching quickjs-ng v0.14.0")
                else()
                    message(STATUS "QuickJS-NG source directory was not set; fetching quickjs-ng v0.14.0")
                endif()
                FetchContent_Declare(
                    quickjs_ng
                    GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
                    GIT_TAG 3c051980ab7e783dfbfb1c70c014ce5e05ecf24c
                    GIT_SHALLOW FALSE
                )
                FetchContent_GetProperties(quickjs_ng)
                if(NOT quickjs_ng_POPULATED)
                    FetchContent_Populate(quickjs_ng)
                endif()
                set(BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR "${quickjs_ng_SOURCE_DIR}")
            else()
                message(FATAL_ERROR
                    "QuickJS-NG 0.14 source was not found at ${BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR}. "
                    "Set BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR to a QuickJS-NG source directory "
                    "or enable BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_AUTO_FETCH."
                )
            endif()
        endif()
        add_library(brookesia_quickjs_ng_wasm STATIC
            "${BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR}/dtoa.c"
            "${BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR}/libregexp.c"
            "${BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR}/libunicode.c"
            "${BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR}/quickjs.c"
            "${BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR}/quickjs-libc.c"
        )
        target_include_directories(brookesia_quickjs_ng_wasm
            PUBLIC
                "${BROOKESIA_RUNTIME_JS_WASM_QUICKJS_NG_DIR}"
        )
        target_compile_definitions(brookesia_quickjs_ng_wasm
            PRIVATE
                QUICKJS_NG_BUILD
                _GNU_SOURCE
        )
        target_compile_options(brookesia_quickjs_ng_wasm
            PRIVATE
                -Wno-pointer-sign
                -Wno-incompatible-pointer-types
                -Wno-cast-function-type
                -Wno-unused-parameter
                -Wno-unused-variable
                -Wno-unused-but-set-variable
                -Wno-sign-compare
                -Wno-missing-field-initializers
                -Wno-type-limits
                -Wno-implicit-fallthrough
                -Wno-format
                -Wno-strict-aliasing
                -Wno-empty-body
        )
    endif()
    if(NOT TARGET brookesia_quickjs_ng_wasm)
        message(FATAL_ERROR "brookesia_runtime_js wasm build requires brookesia_quickjs_ng_wasm")
    endif()
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            brookesia_quickjs_ng_wasm
            m
    )
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            BROOKESIA_RUNTIME_JS_HAS_QUICKJS=1
    )
elseif(QUICKJS_INCLUDE_DIR AND QUICKJS_LIBRARY)
    target_include_directories(${COMPONENT_LIB}
        SYSTEM PRIVATE
            ${QUICKJS_INCLUDE_DIR}
    )
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            ${QUICKJS_LIBRARY}
            m
            dl
            pthread
    )
    if(QUICKJS_LIBC_LIBRARY)
        target_link_libraries(${COMPONENT_LIB} PUBLIC ${QUICKJS_LIBC_LIBRARY})
    endif()
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            BROOKESIA_RUNTIME_JS_HAS_QUICKJS=1
    )
else()
    message(FATAL_ERROR
        "QuickJS was not found; brookesia_runtime_js cannot start JavaScript apps without it. "
        "Build quickjs-ng (see simulator/runtime/README.md), then either:\n"
        "  - set QUICKJS_ROOT to a prefix with include/quickjs.h and lib/libquickjs.a (or lib/libqjs.a), or\n"
        "  - set QUICKJS_ROOT to the quickjs-ng source directory and QUICKJS_BUILD_DIR to the CMake build dir "
        "containing libqjs.a (e.g. .../quickjs-ng/build-pc)."
    )
endif()

if(DEFINED BROOKESIA_RUNTIME_JS_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_RUNTIME_JS_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_RUNTIME_JS_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_RUNTIME_JS_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_RUNTIME_JS_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_RUNTIME_JS_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::runtime_js ALIAS ${COMPONENT_LIB})
