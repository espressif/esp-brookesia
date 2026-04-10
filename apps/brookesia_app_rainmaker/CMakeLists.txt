# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

set(PROJ_SRC ${CMAKE_CURRENT_LIST_DIR})

file(GLOB_RECURSE PROJ_SRCS_C ${PROJ_SRC}/*.c)
file(GLOB_RECURSE PROJ_SRCS_CPP ${PROJ_SRC}/*.cpp)
list(FILTER PROJ_SRCS_C EXCLUDE REGEX ".*/pc/.*")
list(FILTER PROJ_SRCS_CPP EXCLUDE REGEX ".*/pc/.*")
# Do not pick up a stray in-component CMake build dir (e.g. build/CMakeFiles/.../CompilerIdC/*.c);
# those are host tests (macOS AvailabilityMacros.h) and break cross-compilation.
list(FILTER PROJ_SRCS_C EXCLUDE REGEX ".*/build/.*")
list(FILTER PROJ_SRCS_CPP EXCLUDE REGEX ".*/build/.*")

idf_component_register(
    # NOTE: CMake globbing may miss newly-added sources in incremental builds.
    # Keep explicit backend sources here to ensure they are always compiled/linked.
    SRCS  ${PROJ_SRCS_C}
          ${PROJ_SRCS_CPP}
    INCLUDE_DIRS ${PROJ_SRC}/backend/include
    PRIV_INCLUDE_DIRS ${PROJ_SRC}/backend/private_inc
    WHOLE_ARCHIVE
)

set_source_files_properties(
    ${PROJ_SRCS_CPP}
    PROPERTIES
        COMPILE_FLAGS "-Wno-missing-field-initializers"
)
