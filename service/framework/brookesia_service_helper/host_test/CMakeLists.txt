cmake_minimum_required(VERSION 3.20)

project(brookesia_service_helper_host_test LANGUAGES CXX)

get_filename_component(brookesia_root "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)

if(NOT TARGET brookesia::lib_utils)
    add_subdirectory(
        ${brookesia_root}/utils/brookesia_lib_utils
        ${CMAKE_BINARY_DIR}/brookesia_lib_utils
    )
endif()

if(NOT TARGET brookesia::service_manager)
    add_subdirectory(
        ${brookesia_root}/service/framework/brookesia_service_manager
        ${CMAKE_BINARY_DIR}/brookesia_service_manager
    )
endif()

if(NOT TARGET brookesia::service_helper)
    add_subdirectory(
        ${brookesia_root}/service/framework/brookesia_service_helper
        ${CMAKE_BINARY_DIR}/brookesia_service_helper
    )
endif()

add_executable(brookesia_service_helper_host_test
    main.cpp
)

target_include_directories(brookesia_service_helper_host_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(brookesia_service_helper_host_test
    PRIVATE
        brookesia::service_helper
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
