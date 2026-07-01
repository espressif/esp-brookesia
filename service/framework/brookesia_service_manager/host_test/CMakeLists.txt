cmake_minimum_required(VERSION 3.20)

project(brookesia_service_manager_host_test LANGUAGES CXX)

if(NOT TARGET brookesia::lib_utils)
    add_subdirectory(
        ${CMAKE_CURRENT_LIST_DIR}/../../../../utils/brookesia_lib_utils
        ${CMAKE_BINARY_DIR}/brookesia_lib_utils
    )
endif()
add_subdirectory(.. brookesia_service_manager)

add_executable(brookesia_service_manager_host_test
    main.cpp
)

target_link_libraries(brookesia_service_manager_host_test
    PRIVATE
        brookesia::service_manager
)

set_target_properties(brookesia_service_manager_host_test PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

enable_testing()
add_test(
    NAME brookesia_service_manager_host_smoke
    COMMAND brookesia_service_manager_host_test
)
