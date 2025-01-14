idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)

# If want to use custom memory allocation functions, refer to the following macros:
if(CONFIG_ESP_BROOKESIA_MEMORY_USE_CUSTOM)
    # Get the exact component name
    idf_build_get_property(build_components BUILD_COMPONENTS)
    foreach(COMPONENT ${build_components})
        if(COMPONENT MATCHES "esp-brookesia" OR COMPONENT MATCHES "espressif__esp-brookesia")
            set(TARGET_COMPONENT ${COMPONENT})
            break()
        endif()
    endforeach()

    # Get the component library
    if(TARGET_COMPONENT STREQUAL "")
        message(FATAL_ERROR "Component 'esp-brookesia' not found.")
    else()
        idf_component_get_property(ESP_BROOKESIA_LIB ${TARGET_COMPONENT} COMPONENT_LIB)
    endif()

    # Add the custom memory allocation functions
    target_compile_options(
        ${ESP_BROOKESIA_LIB}
        PUBLIC
            "-DESP_BROOKESIA_MEMORY_CUSTOM_MALLOC(x)=heap_caps_aligned_alloc(1, x, MALLOC_CAP_SPIRAM)"  "-DESP_BROOKESIA_MEMORY_CUSTOM_FREE=free"
    )
    set(TARGET_COMPONENT "")
endif()

if(CONFIG_LV_MEM_CUSTOM)
    # Get the exact component name
    idf_build_get_property(build_components BUILD_COMPONENTS)
    foreach(COMPONENT ${build_components})
        if(COMPONENT MATCHES "lvgl" OR COMPONENT MATCHES "lvgl__lvgl")
            set(TARGET_COMPONENT ${COMPONENT})
            break()
        endif()
    endforeach()

    # Get the component library
    if(TARGET_COMPONENT STREQUAL "")
        message(FATAL_ERROR "Component 'lvgl' not found.")
    else()
        idf_component_get_property(LVGL_LIB ${TARGET_COMPONENT} COMPONENT_LIB)
    endif()

    target_compile_options(
        ${LVGL_LIB}
        PUBLIC
            "-DLV_MEM_CUSTOM_ALLOC(x)=heap_caps_malloc(x, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)"
    )
    set(TARGET_COMPONENT "")
endif()

# Get the exact component name
idf_build_get_property(build_components BUILD_COMPONENTS)
foreach(COMPONENT ${build_components})
    if(COMPONENT MATCHES "settings" OR COMPONENT MATCHES "espressif__settings")
        set(TARGET_COMPONENT ${COMPONENT})
        break()
    endif()
endforeach()

# Get the component library
if(TARGET_COMPONENT STREQUAL "")
    message(FATAL_ERROR "Component 'settings' not found.")
else()
    idf_component_get_property(SETTINGS_LIB ${TARGET_COMPONENT} COMPONENT_LIB)
endif()

target_compile_options(
    ${SETTINGS_LIB}
    PUBLIC
        "-DESP_BROOKESIA_APP_PHONE_SETTINGS_WLAN_ENABLE=1" "-DWLAN_DEFAULT_SSID=\"TP-LINK_Liu\"" "-DWLAN_DEFAULT_PWD=\"11112222\""
)
set(TARGET_COMPONENT "")
