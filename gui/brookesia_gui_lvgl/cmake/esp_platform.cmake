#
# ESP Platform
#
idf_component_register(
    SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    REQUIRES ${COMPONENT_REQUIRES}
)

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})

# lv_ftsystem.c includes FreeType internal headers, which are only visible when
# compiling as part of the FreeType library.
idf_build_get_property(build_components BUILD_COMPONENTS)
set(lvgl_component "")
if("lvgl" IN_LIST build_components)
    set(lvgl_component "lvgl")
elseif("lvgl__lvgl" IN_LIST build_components)
    set(lvgl_component "lvgl__lvgl")
endif()
set(freetype_component "")
if("espressif__freetype" IN_LIST build_components)
    set(freetype_component "espressif__freetype")
elseif("freetype" IN_LIST build_components)
    set(freetype_component "freetype")
endif()
if(lvgl_component AND freetype_component)
    idf_component_get_property(lvgl_lib ${lvgl_component} COMPONENT_LIB)
    idf_component_get_property(freetype_lib ${freetype_component} COMPONENT_LIB)
    idf_component_get_property(lvgl_dir ${lvgl_component} COMPONENT_DIR)
    target_link_libraries(${lvgl_lib} PRIVATE ${freetype_lib})
    set(ftsystem_srcs "")
    get_target_property(lvgl_srcs ${lvgl_lib} SOURCES)
    if(lvgl_srcs)
        foreach(lvgl_src ${lvgl_srcs})
            if(lvgl_src MATCHES "lv_ftsystem\\.c$")
                list(APPEND ftsystem_srcs "${lvgl_src}")
            endif()
        endforeach()
    endif()
    list(APPEND ftsystem_srcs
        "${lvgl_dir}/src/libs/freetype/lv_ftsystem.c"
        "${lvgl_dir}/src/font/freetype/lv_ftsystem.c"
        "src/libs/freetype/lv_ftsystem.c"
        "src/font/freetype/lv_ftsystem.c"
    )
    list(REMOVE_DUPLICATES ftsystem_srcs)
    foreach(ftsystem_src ${ftsystem_srcs})
        set_property(SOURCE "${ftsystem_src}" TARGET_DIRECTORY ${lvgl_lib} APPEND PROPERTY COMPILE_DEFINITIONS
            FT2_BUILD_LIBRARY
        )
    endforeach()
endif()
