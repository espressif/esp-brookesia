set(
    BROOKESIA_TEST_BOOST_ROOT
    ""
    CACHE PATH "Optional Boost source root used when system Boost.Test is unavailable"
)

if(NOT TARGET brookesia::lib_utils)
    add_subdirectory(
        ${TEST_APP_DIR}/../../../utils/brookesia_lib_utils
        ${CMAKE_BINARY_DIR}/brookesia_lib_utils
    )
endif()
if(NOT TARGET brookesia::runtime_manager)
    add_subdirectory(
        ${TEST_APP_DIR}/../../brookesia_runtime_manager
        ${CMAKE_BINARY_DIR}/brookesia_runtime_manager
    )
endif()

set(BROOKESIA_RUNTIME_WASM_TEST_APPS_HAS_BACKEND OFF)
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
if(WASMTIME_INCLUDE_DIR AND WASMTIME_LIBRARY)
    add_subdirectory(${TEST_APP_DIR}/.. ${CMAKE_BINARY_DIR}/brookesia_runtime_wasm)
    set(BROOKESIA_RUNTIME_WASM_TEST_APPS_HAS_BACKEND ON)
else()
    message(STATUS "Wasmtime C API not found; runtime_wasm test app will compile public headers and skip backend execution")
endif()

file(GLOB TEST_APP_SRCS_CPP ${TEST_APP_MAIN_DIR}/*.cpp)
add_executable(test_brookesia_runtime_wasm ${TEST_APP_SRCS_CPP})

find_package(Boost COMPONENTS unit_test_framework QUIET)
if(Boost_unit_test_framework_FOUND)
    target_link_libraries(test_brookesia_runtime_wasm PRIVATE Boost::unit_test_framework)
    target_compile_definitions(test_brookesia_runtime_wasm PRIVATE BOOST_TEST_DYN_LINK)
else()
    if("${BROOKESIA_TEST_BOOST_ROOT}" STREQUAL "")
        message(FATAL_ERROR "Boost.Test not found. Set BROOKESIA_TEST_BOOST_ROOT to an esp-boost/src path.")
    endif()
    target_include_directories(test_brookesia_runtime_wasm PRIVATE ${BROOKESIA_TEST_BOOST_ROOT})
    target_compile_definitions(test_brookesia_runtime_wasm PRIVATE
        BROOKESIA_LIB_UTILS_TEST_ADAPTER_USE_INCLUDED_BOOST_TEST=1
    )
endif()

target_compile_features(test_brookesia_runtime_wasm PRIVATE cxx_std_23)
target_include_directories(
    test_brookesia_runtime_wasm
    PRIVATE
        ${TEST_APP_MAIN_DIR}
        ${TEST_APP_DIR}/../include
)
target_link_libraries(test_brookesia_runtime_wasm PRIVATE brookesia::runtime_manager)
if(BROOKESIA_RUNTIME_WASM_TEST_APPS_HAS_BACKEND)
    target_link_libraries(test_brookesia_runtime_wasm PRIVATE brookesia::runtime_wasm)
    target_compile_definitions(test_brookesia_runtime_wasm PRIVATE BROOKESIA_RUNTIME_WASM_TEST_APPS_HAS_BACKEND=1)
else()
    target_compile_definitions(test_brookesia_runtime_wasm PRIVATE BROOKESIA_RUNTIME_WASM_TEST_APPS_HAS_BACKEND=0)
endif()

enable_testing()
add_test(NAME test_brookesia_runtime_wasm COMMAND test_brookesia_runtime_wasm)
