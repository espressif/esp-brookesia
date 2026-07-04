cmake_minimum_required(VERSION 3.20)

project(brookesia_hal_linux_host_test LANGUAGES CXX)

if(NOT DEFINED BROOKESIA_HAL_LINUX_MEDIA_BACKEND)
    set(BROOKESIA_HAL_LINUX_MEDIA_BACKEND "stub" CACHE STRING "HAL Linux media backend")
endif()
if(NOT DEFINED BROOKESIA_HAL_LINUX_VIDEO_BACKEND)
    set(BROOKESIA_HAL_LINUX_VIDEO_BACKEND "stub" CACHE STRING "HAL Linux video backend")
endif()
if(NOT DEFINED BROOKESIA_HAL_LINUX_WIFI_BACKEND)
    set(BROOKESIA_HAL_LINUX_WIFI_BACKEND "stub" CACHE STRING "HAL Linux Wi-Fi backend")
endif()
if(NOT DEFINED BROOKESIA_HAL_LINUX_DISPLAY_BACKEND)
    set(BROOKESIA_HAL_LINUX_DISPLAY_BACKEND "stub" CACHE STRING "HAL Linux display backend")
endif()
if(NOT DEFINED BROOKESIA_HAL_LINUX_POWER_BACKEND)
    set(BROOKESIA_HAL_LINUX_POWER_BACKEND "stub" CACHE STRING "HAL Linux power backend")
endif()

add_subdirectory(.. brookesia_hal_linux)

add_executable(brookesia_hal_linux_host_test
    main.cpp
)

target_link_libraries(brookesia_hal_linux_host_test
    PRIVATE
        brookesia::hal_linux
)

set_target_properties(brookesia_hal_linux_host_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

enable_testing()
option(BROOKESIA_HAL_LINUX_HOST_TEST_ENABLE_REAL_SMOKE "Enable optional non-deterministic real backend smoke test" OFF)
if(NOT BROOKESIA_HAL_LINUX_HOST_TEST_ENABLE_REAL_SMOKE)
    add_test(
        NAME brookesia_hal_linux_smoke
        COMMAND brookesia_hal_linux_host_test
    )
else()
    add_executable(brookesia_hal_linux_real_smoke
        real_smoke.cpp
    )
    target_link_libraries(brookesia_hal_linux_real_smoke
        PRIVATE
            brookesia::hal_linux
    )
    set_target_properties(brookesia_hal_linux_real_smoke PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    add_test(
        NAME brookesia_hal_linux_real_smoke
        COMMAND brookesia_hal_linux_real_smoke
    )
endif()
