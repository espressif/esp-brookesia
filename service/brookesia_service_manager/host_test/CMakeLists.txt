cmake_minimum_required(VERSION 3.20)

project(brookesia_service_manager_host_test LANGUAGES CXX)

add_subdirectory(.. brookesia_service_manager)

add_executable(brookesia_service_manager_host_test
    main.cpp
)

target_link_libraries(brookesia_service_manager_host_test
    PRIVATE
        brookesia_service_manager
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
