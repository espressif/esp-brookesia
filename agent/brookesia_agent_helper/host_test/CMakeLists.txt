cmake_minimum_required(VERSION 3.20)

project(brookesia_agent_helper_host_test LANGUAGES CXX)

add_subdirectory(../../../service/brookesia_service_manager brookesia_service_manager)

add_executable(brookesia_agent_helper_host_test
    main.cpp
)

target_include_directories(brookesia_agent_helper_host_test
    PRIVATE
        ../include
        ../../../service/brookesia_service_helper/include
        ../../../service/brookesia_service_helper/host_test
)

target_link_libraries(brookesia_agent_helper_host_test
    PRIVATE
        brookesia_service_manager
)

set_target_properties(brookesia_agent_helper_host_test PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

enable_testing()
add_test(
    NAME brookesia_agent_helper_schema_dump
    COMMAND brookesia_agent_helper_host_test
)
