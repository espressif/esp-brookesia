set(PROJ_SRC ${CMAKE_CURRENT_LIST_DIR}/src)

set(UI_DIR ${PROJ_SRC}/ui)
file(GLOB_RECURSE UI_SRCS_C ${UI_DIR}/*.c)

file(GLOB PROJ_SRCS_C ${PROJ_SRC}/*.c)
file(GLOB PROJ_SRCS_CPP ${PROJ_SRC}/*.cpp)

idf_component_register(
    SRCS  ${UI_SRCS_C} ${PROJ_SRCS_C} ${PROJ_SRCS_CPP}
    INCLUDE_DIRS ${PROJ_SRC})

set_source_files_properties(
    ${PROJ_SRCS_CPP}
    PROPERTIES
        COMPILE_FLAGS "-Wno-missing-field-initializers"
)
