idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS ".")

# If want to use custom memory allocation functions, refer to the following macros:
if(CONFIG_ESP_UI_MEMORY_USE_CUSTOM)
    # Get the exact component name
    idf_build_get_property(build_components BUILD_COMPONENTS)
    foreach(COMPONENT ${build_components})
        if(COMPONENT MATCHES "esp-ui" OR COMPONENT MATCHES "espressif__esp-ui")
            set(TARGET_COMPONENT ${COMPONENT})
            break()
        endif()
    endforeach()

    # Get the component library
    if(TARGET_COMPONENT STREQUAL "")
        message(FATAL_ERROR "Component 'esp-ui' not found.")
    else()
        idf_component_get_property(ESP_UI_LIB ${TARGET_COMPONENT} COMPONENT_LIB)
    endif()

    # Add the custom memory allocation functions
    target_compile_options(
        ${ESP_UI_LIB}
        PUBLIC
            "-DESP_UI_MEMORY_CUSTOM_MALLOC(x)=heap_caps_aligned_alloc(1, x, MALLOC_CAP_SPIRAM)"  "-DESP_UI_MEMORY_CUSTOM_FREE=free"
    )
endif()
