idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
)

target_compile_options(${COMPONENT_LIB}
    PUBLIC
        -Wno-missing-field-initializers -Wno-unused-variable -Wunused-but-set-variable
)
