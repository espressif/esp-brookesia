cmake_minimum_required(VERSION 3.20)

project(brookesia_system_core_host_test LANGUAGES CXX)

get_filename_component(brookesia_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

set(BROOKESIA_BUILD_LIB_UTILS ON CACHE BOOL "" FORCE)
set(BROOKESIA_BUILD_HAL_INTERFACE ON CACHE BOOL "" FORCE)
set(BROOKESIA_BUILD_SERVICE_MANAGER ON CACHE BOOL "" FORCE)
set(BROOKESIA_BUILD_SERVICE_HELPER ON CACHE BOOL "" FORCE)
set(BROOKESIA_BUILD_GUI_INTERFACE ON CACHE BOOL "" FORCE)
set(BROOKESIA_GUI_INTERFACE_PC_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BROOKESIA_BUILD_RUNTIME_MANAGER ON CACHE BOOL "" FORCE)
set(BROOKESIA_BUILD_SYSTEM_CORE ON CACHE BOOL "" FORCE)

add_subdirectory("${brookesia_root}" esp-brookesia)

add_executable(brookesia_system_core_host_test
    main.cpp
)

add_executable(brookesia_system_core_gate_integration_test
    integration.cpp
)

target_include_directories(brookesia_system_core_host_test
    PRIVATE
        ${brookesia_root}/system/brookesia_system_core/src
)

target_include_directories(brookesia_system_core_gate_integration_test
    PRIVATE
        ${brookesia_root}/system/brookesia_system_core/src
)

target_link_libraries(brookesia_system_core_host_test
    PRIVATE
        brookesia::system_core
)

target_link_libraries(brookesia_system_core_gate_integration_test
    PRIVATE
        brookesia::system_core
)

if(NOT MSVC)
    target_compile_options(brookesia_system_core_host_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(brookesia_system_core_gate_integration_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(brookesia_system_core_host_test PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

set_target_properties(brookesia_system_core_gate_integration_test PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

enable_testing()
add_test(
    NAME brookesia_system_core_service_requirements
    COMMAND brookesia_system_core_host_test
)
add_test(
    NAME brookesia_system_core_install_start_gates
    COMMAND brookesia_system_core_gate_integration_test
)

brookesia_compile_tuning_apply_current_project()
