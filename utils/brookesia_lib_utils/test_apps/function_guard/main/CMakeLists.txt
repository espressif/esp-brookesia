idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity esp_psram
    WHOLE_ARCHIVE
)

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)
