cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS ${EXTRA_COMPONENT_DIRS} "../../gmf_core")

project(test_gmf_core)
