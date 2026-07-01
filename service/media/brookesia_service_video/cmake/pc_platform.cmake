#
# PC Platform
#
set(COMPONENT_LIB brookesia_service_video_impl)
set(component_pc_config_compile_definitions "")

option(
    BROOKESIA_SERVICE_VIDEO_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_DEBUG_LOG on PC"
    OFF
)
option(
    BROOKESIA_SERVICE_VIDEO_PC_CONFIG_ENABLE_ENCODER
    "Default value of CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_ENCODER on PC"
    ON
)
set(
    BROOKESIA_SERVICE_VIDEO_PC_CONFIG_ENCODER_DEFAULT_NUM
    1
    CACHE STRING
    "Default value of CONFIG_BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM on PC"
)
option(
    BROOKESIA_SERVICE_VIDEO_PC_CONFIG_ENABLE_DECODER
    "Default value of CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_DECODER on PC"
    ON
)
set(
    BROOKESIA_SERVICE_VIDEO_PC_CONFIG_DECODER_DEFAULT_NUM
    1
    CACHE STRING
    "Default value of CONFIG_BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM on PC"
)

if(NOT TARGET brookesia::service_display)
    add_subdirectory(
        ${COMPONENT_DIR}/../brookesia_service_display
        ${CMAKE_BINARY_DIR}/brookesia_service_display
    )
endif()

if(BROOKESIA_SERVICE_VIDEO_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_SERVICE_VIDEO_PC_CONFIG_ENABLE_ENCODER)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_ENCODER=1)
    list(APPEND component_pc_config_compile_definitions
        CONFIG_BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM=${BROOKESIA_SERVICE_VIDEO_PC_CONFIG_ENCODER_DEFAULT_NUM}
    )
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_ENCODER=0)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM=0)
endif()

if(BROOKESIA_SERVICE_VIDEO_PC_CONFIG_ENABLE_DECODER)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_DECODER=1)
    list(APPEND component_pc_config_compile_definitions
        CONFIG_BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM=${BROOKESIA_SERVICE_VIDEO_PC_CONFIG_DECODER_DEFAULT_NUM}
    )
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_DECODER=0)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM=0)
endif()

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)
add_library(brookesia::service_video ALIAS ${COMPONENT_LIB})

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        BROOKESIA_SERVICE_VIDEO_VER_MAJOR=${COMPONENT_VERSION_MAJOR}
        BROOKESIA_SERVICE_VIDEO_VER_MINOR=${COMPONENT_VERSION_MINOR}
        BROOKESIA_SERVICE_VIDEO_VER_PATCH=${COMPONENT_VERSION_PATCH}
        ${component_pc_config_compile_definitions}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::service_display
)

if(BROOKESIA_SERVICE_VIDEO_PC_CONFIG_ENABLE_ENCODER AND
    BROOKESIA_SERVICE_VIDEO_PC_CONFIG_ENCODER_DEFAULT_NUM GREATER 0)
    math(EXPR _last "${BROOKESIA_SERVICE_VIDEO_PC_CONFIG_ENCODER_DEFAULT_NUM} - 1")
    foreach(_i RANGE 0 ${_last})
        set(_sym "service_video_encoder_symbol_${_i}")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            BROOKESIA_SERVICE_VIDEO_ENCODER_PLUGIN_SYMBOL_${_i}=${_sym}
        )
        target_link_libraries(${COMPONENT_LIB} PUBLIC "-u ${_sym}")
    endforeach()
    unset(_last)
    unset(_i)
    unset(_sym)
endif()

if(BROOKESIA_SERVICE_VIDEO_PC_CONFIG_ENABLE_DECODER AND
    BROOKESIA_SERVICE_VIDEO_PC_CONFIG_DECODER_DEFAULT_NUM GREATER 0)
    math(EXPR _last "${BROOKESIA_SERVICE_VIDEO_PC_CONFIG_DECODER_DEFAULT_NUM} - 1")
    foreach(_i RANGE 0 ${_last})
        set(_sym "service_video_decoder_symbol_${_i}")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            BROOKESIA_SERVICE_VIDEO_DECODER_PLUGIN_SYMBOL_${_i}=${_sym}
        )
        target_link_libraries(${COMPONENT_LIB} PUBLIC "-u ${_sym}")
    endforeach()
    unset(_last)
    unset(_i)
    unset(_sym)
endif()

if(DEFINED BROOKESIA_SERVICE_VIDEO_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_SERVICE_VIDEO_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SERVICE_VIDEO_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_SERVICE_VIDEO_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_SERVICE_VIDEO_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_SERVICE_VIDEO_PC_COMPILE_OPTIONS}
    )
endif()
