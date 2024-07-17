idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS ".")

if(CONFIG_ESP_UI_MEMORY_USE_CUSTOM)
    idf_component_get_property(ESP_UI_LIB esp-ui COMPONENT_LIB)
    target_compile_options(
        ${ESP_UI_LIB}
        PUBLIC
            "-DESP_UI_MEMORY_CUSTOM_MALLOC(x)=heap_caps_aligned_alloc(1, x, MALLOC_CAP_SPIRAM)"  "-DESP_UI_MEMORY_CUSTOM_FREE=free"
    )
endif()
