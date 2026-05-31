cmake_minimum_required(VERSION 3.20)

project(brookesia_service_helper_host_test LANGUAGES CXX)

add_subdirectory(../../../utils/brookesia_lib_utils brookesia_lib_utils)
if((NOT TARGET brookesia_lib_utils) AND TARGET brookesia_lib_utils_impl)
    add_library(brookesia_lib_utils ALIAS brookesia_lib_utils_impl)
endif()

add_subdirectory(../../brookesia_service_manager brookesia_service_manager)

add_executable(brookesia_service_helper_host_test
    main.cpp
)

target_include_directories(brookesia_service_helper_host_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ../include
        ../../../hal/brookesia_hal_interface/include
)

target_link_libraries(brookesia_service_helper_host_test
    PRIVATE
        brookesia_service_manager
)

set_target_properties(brookesia_service_helper_host_test PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

enable_testing()
add_test(
    NAME brookesia_service_helper_schema_dump
    COMMAND brookesia_service_helper_host_test
)
