file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_LIST_DIR}/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/*.c")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-function)

# Check for CI test WiFi credentials environment variables
if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP1_SSID})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        EXAMPLE_WIFI_SSID="$ENV{CI_TEST_WIFI_2_4G_AP1_SSID}"
    )
endif()
if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP1_PSW})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        EXAMPLE_WIFI_PSW="$ENV{CI_TEST_WIFI_2_4G_AP1_PSW}"
    )
endif()
