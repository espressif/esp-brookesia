set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_SRC_DIR ${COMPONENT_DIR}/src)
set(COMPONENT_INCLUDE_DIRS ${COMPONENT_DIR}/include)
set(COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_SRC_DIR})
set(COMPONENT_REQUIRES "")
set(COMPONENT_PRIV_REQUIRES "")
set(COMPONENT_SRCS_C "")
set(COMPONENT_SRCS_CPP "")
set(COMPONENT_SRCS_C_COMPILE_FLAGS "")
set(COMPONENT_SRCS_CPP_COMPILE_FLAGS "")
set(COMPONENT_SRCS_COMPILE_OPTIONS "")

#
# Service
#
file(GLOB SERVICE_SRCS_C ${COMPONENT_SRC_DIR}/*.c)
file(GLOB SERVICE_SRCS_CPP ${COMPONENT_SRC_DIR}/*.cpp)
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
        PRIV_REQUIRES ${COMPONENT_PRIV_REQUIRES}
    )
    if(CONFIG_BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_REGISTER)
        set(BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL service_wifi_symbol)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL=${BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL}
        )
        target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL}")
    endif()

    include(package_manager)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
else()
    #
    # PC Platform
    #
    set(component_pc_config_compile_definitions "")
    option(
        BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_REGISTER
        "Default value of CONFIG_BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_REGISTER on PC"
        ON
    )
    option(
        BROOKESIA_SERVICE_WIFI_PC_CONFIG_ENABLE_DEBUG_LOG
        "Default value of CONFIG_BROOKESIA_SERVICE_WIFI_ENABLE_DEBUG_LOG on PC"
        OFF
    )

    if(BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_REGISTER)
        set(BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL service_wifi_symbol)
        list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_REGISTER=1)
        list(APPEND component_pc_config_compile_definitions
            BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL=${BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL}
        )
    else()
        list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_REGISTER=0)
    endif()
    if(BROOKESIA_SERVICE_WIFI_PC_CONFIG_ENABLE_DEBUG_LOG)
        list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_WIFI_ENABLE_DEBUG_LOG=1)
    else()
        list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_WIFI_ENABLE_DEBUG_LOG=0)
    endif()

    if(NOT TARGET brookesia::lib_utils)
        add_subdirectory(
            ${COMPONENT_DIR}/../../utils/brookesia_lib_utils
            ${CMAKE_BINARY_DIR}/brookesia_lib_utils
        )
    endif()
    if(NOT TARGET brookesia::service_manager)
        add_subdirectory(
            /../../framework/brookesia_service_manager
            ${CMAKE_BINARY_DIR}/brookesia_service_manager
        )
    endif()
    if(NOT TARGET brookesia::hal_interface)
        add_subdirectory(
            ${COMPONENT_DIR}/../../hal/brookesia_hal_interface
            ${CMAKE_BINARY_DIR}/brookesia_hal_interface
        )
    endif()
    if(NOT TARGET brookesia::service_helper)
        add_subdirectory(
            /../../framework/brookesia_service_helper
            ${CMAKE_BINARY_DIR}/brookesia_service_helper
        )
    endif()

    if(NOT EMSCRIPTEN)
        find_package(Boost REQUIRED COMPONENTS thread system chrono json)
    endif()

    set(COMPONENT_LIB brookesia_service_wifi)
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
    if(Boost_INCLUDE_DIRS)
        target_include_directories(${COMPONENT_LIB} PUBLIC ${Boost_INCLUDE_DIRS})
    endif()
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            brookesia::service_manager
            brookesia::service_helper
            brookesia::hal_interface
            brookesia::lib_utils
    )
    if(NOT EMSCRIPTEN)
        target_link_libraries(${COMPONENT_LIB}
            PUBLIC
                Boost::thread
                Boost::system
                Boost::chrono
                Boost::json
        )
    endif()
    if(BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_REGISTER)
        target_link_libraries(${COMPONENT_LIB} PUBLIC "-u ${BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL}")
    endif()
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${component_pc_config_compile_definitions}
    )
    add_library(brookesia::service_wifi ALIAS ${COMPONENT_LIB})
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
