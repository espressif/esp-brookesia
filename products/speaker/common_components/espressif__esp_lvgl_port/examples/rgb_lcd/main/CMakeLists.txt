idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "." ${LV_DEMO_DIR})

# lvgl_port_create_c_image("images/esp_logo.png" "images/" "RGB565" "NONE")
file(GLOB PNG_FILES "images/*.png")
foreach(PNG_FILE ${PNG_FILES})
    get_filename_component(PNG_NAME ${PNG_FILE} NAME)
    if(PNG_NAME MATCHES "wallpaper")
        lvgl_port_create_c_image("images/${PNG_NAME}" "images/" "RGB888" "NONE")
    else()
        lvgl_port_create_c_image("images/${PNG_NAME}" "images/" "ARGB8888" "NONE")
    endif()
endforeach()
lvgl_port_add_images(${COMPONENT_LIB} "images/")

set_source_files_properties(
    PROPERTIES COMPILE_OPTIONS
    "-DLV_LVGL_H_INCLUDE_SIMPLE;-Wno-format;"
    )
