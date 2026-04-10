idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity test_class test_plugin_macro_a test_plugin_macro_a_custom
                  test_plugin_macro_singleton_a test_plugin_macro_singleton_b_custom
                  test_plugin_macro_unused
    WHOLE_ARCHIVE
)

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)
