cmake_minimum_required(VERSION 3.16)

include("${CMAKE_CURRENT_LIST_DIR}/../common.cmake")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(DEPENDENCIES_LOCK "${DEPENDENCIES_LOCK}")
project(example_system_gsp_app)
brookesia_system_check_build_backend()

brookesia_compile_tuning_apply_current_project()
