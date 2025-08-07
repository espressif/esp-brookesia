set(PROJ_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB SRCS_C ${PROJ_SRC_DIR}/*.c)
file(GLOB SRCS_CPP ${PROJ_SRC_DIR}/*.cpp)

set(MANAGER_SRC_DIR ${PROJ_SRC_DIR}/manager)
file(GLOB_RECURSE MANAGER_SRCS_C ${MANAGER_SRC_DIR}/*.c)
file(GLOB_RECURSE MANAGER_SRCS_CPP ${MANAGER_SRC_DIR}/*.cpp)
list(APPEND SRCS_C ${MANAGER_SRCS_C})
list(APPEND SRCS_CPP ${MANAGER_SRCS_CPP})

set(ASSETS_SRC_DIR ${PROJ_SRC_DIR}/assets)
file(GLOB_RECURSE ASSETS_SRCS_C ${ASSETS_SRC_DIR}/*.c)
list(APPEND SRCS_C ${ASSETS_SRCS_C})

set(UI_SRC_DIR ${PROJ_SRC_DIR}/ui)
file(GLOB_RECURSE UI_SRCS_C ${UI_SRC_DIR}/*.c)
file(GLOB_RECURSE UI_SRCS_CPP ${UI_SRC_DIR}/*.cpp)
list(APPEND SRCS_C ${UI_SRCS_C})
list(APPEND SRCS_CPP ${UI_SRCS_CPP})

idf_component_register(
    SRCS ${SRCS_C} ${SRCS_CPP}
    INCLUDE_DIRS ${PROJ_SRC_DIR}
    REQUIRES esp_event esp_wifi esp_http_server json spi_flash
    EMBED_TXTFILES ${PROJ_SRC_DIR}/html/wifi.html
    WHOLE_ARCHIVE
)

set_source_files_properties(
    ${SRCS_CPP}
    PROPERTIES
        COMPILE_FLAGS "-Wno-missing-field-initializers -Wno-deprecated-enum-enum-conversion"
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE -DLV_LVGL_H_INCLUDE_SIMPLE)
