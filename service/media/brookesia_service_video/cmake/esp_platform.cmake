idf_component_register(
    SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    REQUIRES ${COMPONENT_REQUIRES}
    PRIV_REQUIRES ${COMPONENT_PRIV_REQUIRES}
)

#
# Encoder auto-registration: generate one symbol per encoder instance
# Symbol name: service_video_encoder_symbol_<i>
#
if(CONFIG_BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM GREATER 0)
    math(EXPR _last "${CONFIG_BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM} - 1")
    foreach(_i RANGE 0 ${_last})
        set(_sym "service_video_encoder_symbol_${_i}")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            BROOKESIA_SERVICE_VIDEO_ENCODER_PLUGIN_SYMBOL_${_i}=${_sym}
        )
        target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${_sym}")
    endforeach()
    unset(_last)
    unset(_i)
    unset(_sym)
endif()

#
# Decoder auto-registration: generate one symbol per decoder instance
# Symbol name: service_video_decoder_symbol_<i>
#
if(CONFIG_BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM GREATER 0)
    math(EXPR _last "${CONFIG_BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM} - 1")
    foreach(_i RANGE 0 ${_last})
        set(_sym "service_video_decoder_symbol_${_i}")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            BROOKESIA_SERVICE_VIDEO_DECODER_PLUGIN_SYMBOL_${_i}=${_sym}
        )
        target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${_sym}")
    endforeach()
    unset(_last)
    unset(_i)
    unset(_sym)
endif()

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})
