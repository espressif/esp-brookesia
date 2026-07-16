idf_component_register(
    SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    REQUIRES ${COMPONENT_REQUIRES}
    PRIV_REQUIRES ${COMPONENT_PRIV_REQUIRES}
)

#
# Service auto-registration: export the plugin symbol so the linker keeps the registrar.
#
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    BROOKESIA_SERVICE_PICODET_PLUGIN_SYMBOL=service_picodet_symbol
)
target_link_libraries(${COMPONENT_LIB} PRIVATE "-u service_picodet_symbol")

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})
