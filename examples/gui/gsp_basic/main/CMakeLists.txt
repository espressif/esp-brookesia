file(GLOB SRCS "${CMAKE_CURRENT_LIST_DIR}/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/*.c")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
)

set(gsp_basic_build_id "unknown")
execute_process(
    COMMAND git -C "${CMAKE_CURRENT_LIST_DIR}" rev-parse --short=12 HEAD
    OUTPUT_VARIABLE gsp_basic_build_id
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND git -C "${CMAKE_CURRENT_LIST_DIR}" status --porcelain
    OUTPUT_VARIABLE gsp_basic_dirty
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(gsp_basic_dirty)
    string(APPEND gsp_basic_build_id "-dirty")
endif()
target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE "BROOKESIA_GSP_BASIC_BUILD_ID=\"${gsp_basic_build_id}\""
)

if(NOT COMMAND gsp_add_bundle)
    idf_build_get_property(_gsp_components BUILD_COMPONENTS)
    set(_gsp_cmake "")
    foreach(_name IN LISTS _gsp_components)
        if(_name MATCHES "esp-gsp$")
            idf_component_get_property(_gsp_dir ${_name} COMPONENT_DIR)
            set(_gsp_cmake "${_gsp_dir}/cmake/gsp_bundle.cmake")
        endif()
    endforeach()
    if(NOT EXISTS "${_gsp_cmake}")
        message(FATAL_ERROR "esp-gsp bundle helper was not found")
    endif()
    include("${_gsp_cmake}")
endif()

gsp_add_bundle(${COMPONENT_LIB}
    SCENES "${CMAKE_CURRENT_LIST_DIR}/scenes/home.json"
    PIXEL_FORMAT rgb565
    SYMBOL demo
)

# esp-gsp compiles its IDF DMA2D adapter in this project. Give that translation
# unit the compatibility name; do not rewrite the downloaded source.
if(NOT TARGET esp_gsp_project_config)
    message(FATAL_ERROR "esp-gsp project config target is unavailable")
endif()
set(gsp_dma2d_compat "${CMAKE_CURRENT_LIST_DIR}/gsp_dma2d_idf60.h")
target_compile_options(
    esp_gsp_project_config PRIVATE
    "SHELL:-include ${gsp_dma2d_compat}"
)
