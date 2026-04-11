file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_LIST_DIR}/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/*.c")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
)
