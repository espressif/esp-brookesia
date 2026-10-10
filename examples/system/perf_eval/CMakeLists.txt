if("${CMAKE_GENERATOR}" MATCHES "Ninja")
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")
endif()

cmake_minimum_required(VERSION 3.16)

option(CCACHE_ENABLE "Use ccache to accelerate repeat ESP-IDF builds" ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(example_system_perf_eval)

# -Wfree-nonheap-object is a GCC 16 false positive that fires only at -O2/-O3.
# CONFIG_COMPILER_OPTIMIZATION_PERF=y is set in sdkconfig.defaults.esp32p4, so
# gui_interface's runtime_style.cpp:434 (resolve_font_chain, a lambda whose
# std::vector<std::string> return is inlined into operator delete) trips it and
# -Werror aborts the build. Super hits the same code at the same level.
#
# This is suppressed here, app-locally, rather than by downgrading the
# optimization level: v1.8 section 1.1 requires the optimized build for the
# Pass-B/optimized grading, so a size-optimized build would invalidate every
# number the app produces. It is also not fixed in the framework, because that
# would mean editing shared GUI code for a compiler-version artifact.
#
# Remove this line once the toolchain no longer reports the false positive, or
# once runtime_style.cpp:481 is restructured so the temporary is not inlined
# into its own deallocation.
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=free-nonheap-object" APPEND)
