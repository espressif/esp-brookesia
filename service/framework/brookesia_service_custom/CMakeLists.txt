set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_SRC_DIR ${COMPONENT_DIR}/src)
set(COMPONENT_INCLUDE_DIRS ${COMPONENT_DIR}/include)
set(COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_SRC_DIR})
set(COMPONENT_REQUIRES "")
set(COMPONENT_SRCS_C "")
set(COMPONENT_SRCS_CPP "")
set(COMPONENT_SRCS_C_COMPILE_FLAGS "")
set(COMPONENT_SRCS_CPP_COMPILE_FLAGS "")
set(COMPONENT_SRCS_COMPILE_OPTIONS "")
set(COMPONENT_VERSION_MAJOR 0)
set(COMPONENT_VERSION_MINOR 7)
set(COMPONENT_VERSION_PATCH 2)

#
# Service
#
file(GLOB SERVICE_SRCS_C ${COMPONENT_SRC_DIR}/*.c)
file(GLOB SERVICE_SRCS_CPP ${COMPONENT_SRC_DIR}/*.cpp)
list(APPEND COMPONENT_SRCS_C ${SERVICE_SRCS_C})
list(APPEND COMPONENT_SRCS_CPP ${SERVICE_SRCS_CPP})

if(ESP_PLATFORM)
    #
    # Register Component
    #
    idf_component_register(
        SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
        INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
        PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
        REQUIRES ${COMPONENT_REQUIRES}
    )
    if(CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER)
        set(BROOKESIA_SERVICE_CUSTOM_PLUGIN_SYMBOL service_custom_symbol)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            BROOKESIA_SERVICE_CUSTOM_PLUGIN_SYMBOL=${BROOKESIA_SERVICE_CUSTOM_PLUGIN_SYMBOL}
        )
        target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_SERVICE_CUSTOM_PLUGIN_SYMBOL}")
    endif()

    include(package_manager)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
else()
    set(COMPONENT_LIB brookesia_service_custom)
    set(component_pc_config_compile_definitions
        BROOKESIA_SERVICE_CUSTOM_VER_MAJOR=${COMPONENT_VERSION_MAJOR}
        BROOKESIA_SERVICE_CUSTOM_VER_MINOR=${COMPONENT_VERSION_MINOR}
        BROOKESIA_SERVICE_CUSTOM_VER_PATCH=${COMPONENT_VERSION_PATCH}
    )

    option(
        BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER
        "Default value of CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER on PC"
        ON
    )
    option(
        BROOKESIA_SERVICE_CUSTOM_PC_CONFIG_ENABLE_DEBUG_LOG
        "Default value of CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_DEBUG_LOG on PC"
        OFF
    )
    option(
        BROOKESIA_SERVICE_CUSTOM_PC_CONFIG_ENABLE_WORKER
        "Default value of CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER on PC"
        OFF
    )

    if(BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER)
        set(BROOKESIA_SERVICE_CUSTOM_PLUGIN_SYMBOL service_custom_symbol)
        list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER=1)
        list(APPEND component_pc_config_compile_definitions
            BROOKESIA_SERVICE_CUSTOM_PLUGIN_SYMBOL=${BROOKESIA_SERVICE_CUSTOM_PLUGIN_SYMBOL}
        )
    else()
        list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER=0)
    endif()
    if(BROOKESIA_SERVICE_CUSTOM_PC_CONFIG_ENABLE_DEBUG_LOG)
        list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_DEBUG_LOG=1)
    else()
        list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_DEBUG_LOG=0)
    endif()
    if(BROOKESIA_SERVICE_CUSTOM_PC_CONFIG_ENABLE_WORKER)
        list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER=1)
    else()
        list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER=0)
    endif()

    if(NOT TARGET brookesia::lib_utils)
        add_subdirectory(
            ${COMPONENT_DIR}/../../utils/brookesia_lib_utils
            ${CMAKE_BINARY_DIR}/brookesia_lib_utils
        )
    endif()
    if(NOT TARGET brookesia::service_manager)
        add_subdirectory(
            ${COMPONENT_DIR}/../brookesia_service_manager
            ${CMAKE_BINARY_DIR}/brookesia_service_manager
        )
    endif()

    find_package(Boost REQUIRED COMPONENTS thread system chrono json)

    add_library(${COMPONENT_LIB} STATIC
        ${COMPONENT_SRCS_C}
        ${COMPONENT_SRCS_CPP}
    )
    add_library(brookesia::service_custom ALIAS ${COMPONENT_LIB})

    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
    target_include_directories(${COMPONENT_LIB}
        PUBLIC
            ${COMPONENT_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS}
        PRIVATE
            ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    )
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            brookesia::service_manager
            brookesia::lib_utils
            Boost::thread
            Boost::system
            Boost::chrono
            Boost::json
    )
    if(BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER)
        target_link_libraries(${COMPONENT_LIB} PUBLIC "-u ${BROOKESIA_SERVICE_CUSTOM_PLUGIN_SYMBOL}")
    endif()
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${component_pc_config_compile_definitions}
    )
endif()

#
# Compile Options
#
if(NOT CMAKE_SCRIPT_MODE_FILE)
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
endif()
