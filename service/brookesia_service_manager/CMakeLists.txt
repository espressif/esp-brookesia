if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_SRC_DIR ${COMPONENT_DIR}/src)
set(COMPONENT_INCLUDE_DIRS ${COMPONENT_DIR}/include)
set(COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_SRC_DIR})
set(COMPONENT_REQUIRES esp_netif)
set(COMPONENT_SRCS_C "")
set(COMPONENT_SRCS_CPP "")
set(COMPONENT_SRCS_C_COMPILE_FLAGS "")
set(COMPONENT_SRCS_CPP_COMPILE_FLAGS "")
set(COMPONENT_SRCS_COMPILE_OPTIONS "")

#
# RPC
#
set(RPC_SRC_DIR ${COMPONENT_SRC_DIR}/rpc)
file(GLOB_RECURSE RPC_SRCS_C ${RPC_SRC_DIR}/*.c)
file(GLOB_RECURSE RPC_SRCS_CPP ${RPC_SRC_DIR}/*.cpp)
list(APPEND COMPONENT_SRCS_C ${RPC_SRCS_C})
list(APPEND COMPONENT_SRCS_CPP ${RPC_SRCS_CPP})

#
# Event
#
set(EVENT_SRC_DIR ${COMPONENT_SRC_DIR}/event)
file(GLOB_RECURSE EVENT_SRCS_C ${EVENT_SRC_DIR}/*.c)
file(GLOB_RECURSE EVENT_SRCS_CPP ${EVENT_SRC_DIR}/*.cpp)
list(APPEND COMPONENT_SRCS_C ${EVENT_SRCS_C})
list(APPEND COMPONENT_SRCS_CPP ${EVENT_SRCS_CPP})

#
# Function
#
set(FUNCTION_SRC_DIR ${COMPONENT_SRC_DIR}/function)
file(GLOB_RECURSE FUNCTION_SRCS_C ${FUNCTION_SRC_DIR}/*.c)
file(GLOB_RECURSE FUNCTION_SRCS_CPP ${FUNCTION_SRC_DIR}/*.cpp)
list(APPEND COMPONENT_SRCS_C ${FUNCTION_SRCS_C})
list(APPEND COMPONENT_SRCS_CPP ${FUNCTION_SRCS_CPP})

#
# Private
#
set(PRIVATE_SRC_DIR ${COMPONENT_SRC_DIR}/private)
file(GLOB_RECURSE PRIVATE_SRCS_C ${PRIVATE_SRC_DIR}/*.c)
file(GLOB_RECURSE PRIVATE_SRCS_CPP ${PRIVATE_SRC_DIR}/*.cpp)
list(APPEND COMPONENT_SRCS_C ${PRIVATE_SRCS_C})
list(APPEND COMPONENT_SRCS_CPP ${PRIVATE_SRCS_CPP})

#
# Service
#
set(SERVICE_SRC_DIR ${COMPONENT_SRC_DIR}/service)
file(GLOB_RECURSE SERVICE_SRCS_C ${SERVICE_SRC_DIR}/*.c)
file(GLOB_RECURSE SERVICE_SRCS_CPP ${SERVICE_SRC_DIR}/*.cpp)
list(APPEND COMPONENT_SRCS_C ${SERVICE_SRCS_C})
list(APPEND COMPONENT_SRCS_CPP ${SERVICE_SRCS_CPP})

if(ESP_PLATFORM)
    #
    # ESP Platform
    #
    idf_component_register(
        SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
        INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
        PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
        REQUIRES ${COMPONENT_REQUIRES}
    )

    include(package_manager)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
else()
    #
    # PC Platform
    #
    if(NOT TARGET brookesia_lib_utils)
        add_subdirectory(
            ${COMPONENT_DIR}/../../utils/brookesia_lib_utils
            ${CMAKE_BINARY_DIR}/brookesia_lib_utils
        )
    endif()

    find_package(Boost REQUIRED COMPONENTS thread system chrono json)

    set(COMPONENT_LIB brookesia_service_manager)
    add_library(${COMPONENT_LIB} STATIC
        ${COMPONENT_SRCS_C}
        ${COMPONENT_SRCS_CPP}
    )

    set_target_properties(${COMPONENT_LIB} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )

    target_include_directories(${COMPONENT_LIB}
        PUBLIC
            ${COMPONENT_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS}
        PRIVATE
            ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    )

    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            brookesia_lib_utils
            Boost::thread
            Boost::system
            Boost::chrono
            Boost::json
    )
endif()

#
# Compile Options
#
target_compile_options(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_SRCS_COMPILE_OPTIONS}
)
set_source_files_properties(${COMPONENT_SRCS_C}
    PROPERTIES
        COMPILE_FLAGS "${COMPONENT_SRCS_C_COMPILE_FLAGS}"
)
set_source_files_properties(${COMPONENT_SRCS_CPP}
    PROPERTIES
        COMPILE_FLAGS "${COMPONENT_SRCS_CPP_COMPILE_FLAGS}"
)
