file(GLOB_RECURSE MAIN_SRCS ./*.cpp ./*.c)
set(EMBEDDED_FILES "")

if (CONFIG_EXAMPLE_COZE_AGENT_PRIVATE_KEY_FILE_PATH)
    list(APPEND EMBEDDED_FILES ${CONFIG_EXAMPLE_COZE_AGENT_PRIVATE_KEY_FILE_PATH})
    message(STATUS "Coze Private Key Path: ${CONFIG_EXAMPLE_COZE_AGENT_PRIVATE_KEY_FILE_PATH}")
endif()

idf_component_register(
    SRCS ${MAIN_SRCS}
    INCLUDE_DIRS "."
    EMBED_TXTFILES ${EMBEDDED_FILES}
)

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)

spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)

# Function to get component library
function(get_component_library component_name output_var)
    # Get the exact component name
    idf_build_get_property(build_components BUILD_COMPONENTS)
    set(TARGET_COMPONENT "")
    foreach(COMPONENT ${build_components})
        if(COMPONENT MATCHES "${component_name}" OR COMPONENT MATCHES "espressif__${component_name}")
            set(TARGET_COMPONENT ${COMPONENT})
            break()
        endif()
    endforeach()

    # Get the component library
    if(TARGET_COMPONENT STREQUAL "")
        message(FATAL_ERROR "Component '${component_name}' not found.")
    else()
        idf_component_get_property(COMPONENT_LIB ${TARGET_COMPONENT} COMPONENT_LIB)
        set(${output_var} ${COMPONENT_LIB} PARENT_SCOPE)
    endif()
endfunction()

get_component_library("lvgl" LVGL_LIB)
if(CONFIG_LV_USE_CUSTOM_MALLOC)
    target_link_libraries(${LVGL_LIB}
        PUBLIC
            "-u lv_mem_init"
            "-u lv_mem_deinit"
    )
elseif(CONFIG_LV_USE_BUILTIN_MALLOC)
    target_compile_options(${LVGL_LIB}
        PUBLIC
            "-DLV_ATTRIBUTE_LARGE_RAM_ARRAY=EXT_RAM_BSS_ATTR"
    )
endif()
if(CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM)
    target_compile_options(
        ${LVGL_LIB}
        PUBLIC
            "-Wno-attributes"
    )
endif()
if(CONFIG_LV_OS_FREERTOS)
    target_compile_options(
        ${LVGL_LIB}
        PUBLIC
            "-Wno-unused-function"
    )
endif()

get_component_library("gmf_core" GMF_CORE_LIB)
target_compile_options(
    ${GMF_CORE_LIB}
    PUBLIC
        "-fpermissive"
)

get_component_library("esp_lcd_touch_cst816s" LCD_TOUCH_LIB)
target_compile_options(
    ${LCD_TOUCH_LIB}
    PUBLIC
        "-Wno-unused-function"
)
