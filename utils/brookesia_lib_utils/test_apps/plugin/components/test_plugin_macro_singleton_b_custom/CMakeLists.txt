idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES "test_class"
)
target_link_libraries(${COMPONENT_LIB} PRIVATE "-u macro_singleton_b_custom_symbol")
