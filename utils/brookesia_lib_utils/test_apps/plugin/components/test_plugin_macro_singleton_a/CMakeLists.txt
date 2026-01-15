idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES "test_class"
)
target_link_libraries(${COMPONENT_LIB} PRIVATE "-u _PluginSingletonA_symbol_10")
