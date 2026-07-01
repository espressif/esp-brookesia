set(COMPONENT_LIB brookesia_emulation_nes)
set(component_pc_config_compile_definitions "")

option(
    BROOKESIA_EMULATION_NES_ENABLE_AUTO_REGISTER
    "Default value of CONFIG_BROOKESIA_EMULATION_NES_ENABLE_AUTO_REGISTER on PC"
    ON
)
option(
    BROOKESIA_EMULATION_NES_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_EMULATION_NES_ENABLE_DEBUG_LOG on PC"
    OFF
)
option(
    BROOKESIA_EMULATION_NES_PC_CONFIG_ENABLE_NOFRENDO
    "Default value of CONFIG_BROOKESIA_EMULATION_NES_ENABLE_NOFRENDO on PC"
    ON
)

set(_BROOKESIA_EMULATION_NES_SRCS_C ${COMPONENT_SRCS_C})
set(_BROOKESIA_EMULATION_NES_SRCS_CPP ${COMPONENT_SRCS_CPP})
set(COMPONENT_SRCS_C "")
set(COMPONENT_SRCS_CPP "")

if(NOT TARGET brookesia::lib_utils)
    add_subdirectory(
        ${COMPONENT_DIR}/../../../utils/brookesia_lib_utils
        ${CMAKE_BINARY_DIR}/brookesia_lib_utils
    )
endif()

if(NOT TARGET brookesia::service_manager)
    add_subdirectory(
        ${COMPONENT_DIR}/../../framework/brookesia_service_manager
        ${CMAKE_BINARY_DIR}/brookesia_service_manager
    )
endif()

if(NOT TARGET brookesia::service_helper)
    add_subdirectory(
        ${COMPONENT_DIR}/../../framework/brookesia_service_helper
        ${CMAKE_BINARY_DIR}/brookesia_service_helper
    )
endif()

if(NOT TARGET brookesia::service_display)
    add_subdirectory(
        ${COMPONENT_DIR}/../../media/brookesia_service_display
        ${CMAKE_BINARY_DIR}/brookesia_service_display
    )
endif()

if(NOT TARGET brookesia::service_audio)
    add_subdirectory(
        ${COMPONENT_DIR}/../../media/brookesia_service_audio
        ${CMAKE_BINARY_DIR}/brookesia_service_audio
    )
endif()

set(COMPONENT_SRCS_C ${_BROOKESIA_EMULATION_NES_SRCS_C})
set(COMPONENT_SRCS_CPP ${_BROOKESIA_EMULATION_NES_SRCS_CPP})

find_package(Boost REQUIRED COMPONENTS thread system chrono json)

if(BROOKESIA_EMULATION_NES_ENABLE_AUTO_REGISTER)
    set(BROOKESIA_EMULATION_NES_PLUGIN_SYMBOL emulation_nes_symbol)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_EMULATION_NES_ENABLE_AUTO_REGISTER=1)
    list(APPEND component_pc_config_compile_definitions
        BROOKESIA_EMULATION_NES_PLUGIN_SYMBOL=${BROOKESIA_EMULATION_NES_PLUGIN_SYMBOL}
    )
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_EMULATION_NES_ENABLE_AUTO_REGISTER=0)
endif()

if(BROOKESIA_EMULATION_NES_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_EMULATION_NES_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_EMULATION_NES_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_EMULATION_NES_PC_CONFIG_ENABLE_NOFRENDO)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_EMULATION_NES_ENABLE_NOFRENDO=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_EMULATION_NES_ENABLE_NOFRENDO=0)
endif()

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)
add_library(brookesia::emulation_nes ALIAS ${COMPONENT_LIB})

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        BROOKESIA_EMULATION_NES_VER_MAJOR=${COMPONENT_VERSION_MAJOR}
        BROOKESIA_EMULATION_NES_VER_MINOR=${COMPONENT_VERSION_MINOR}
        BROOKESIA_EMULATION_NES_VER_PATCH=${COMPONENT_VERSION_PATCH}
        ${component_pc_config_compile_definitions}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::service_manager
        brookesia::service_helper
        brookesia::service_display
        brookesia::service_audio
        brookesia::lib_utils
        Boost::thread
        Boost::system
        Boost::chrono
        Boost::json
)

if(BROOKESIA_EMULATION_NES_ENABLE_AUTO_REGISTER)
    target_link_libraries(${COMPONENT_LIB} PUBLIC "-u ${BROOKESIA_EMULATION_NES_PLUGIN_SYMBOL}")
endif()

if(DEFINED BROOKESIA_EMULATION_NES_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_EMULATION_NES_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_EMULATION_NES_PC_COMPILE_OPTIONS}
    )
endif()
