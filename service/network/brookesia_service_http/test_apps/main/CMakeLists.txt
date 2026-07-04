idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE TRUE
)

littlefs_create_partition_image(littlefs_data ../littlefs FLASH_IN_PROJECT)

if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP1_SSID})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        TEST_HTTP_ENABLE_NETWORK=1
        TEST_HTTP_USE_WIFI=1
        TEST_HTTP_WIFI_SSID="$ENV{CI_TEST_WIFI_2_4G_AP1_SSID}"
    )
    if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP1_PSW})
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            TEST_HTTP_WIFI_PASSWORD="$ENV{CI_TEST_WIFI_2_4G_AP1_PSW}"
        )
    endif()
    if(DEFINED ENV{TEST_HTTP_URL})
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            TEST_HTTP_URL="$ENV{TEST_HTTP_URL}"
        )
    endif()
    if(DEFINED ENV{TEST_HTTP_ECHO_URL})
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            TEST_HTTP_ECHO_URL="$ENV{TEST_HTTP_ECHO_URL}"
        )
    endif()
endif()

if(DEFINED ENV{TEST_HTTP_STRESS_SEQUENTIAL_COUNT})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        "TEST_HTTP_STRESS_SEQUENTIAL_COUNT=($ENV{TEST_HTTP_STRESS_SEQUENTIAL_COUNT})"
    )
endif()

if(DEFINED ENV{TEST_HTTP_STRESS_CONCURRENT_ROUNDS})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        "TEST_HTTP_STRESS_CONCURRENT_ROUNDS=($ENV{TEST_HTTP_STRESS_CONCURRENT_ROUNDS})"
    )
endif()

if(DEFINED ENV{TEST_HTTP_STRESS_CONCURRENT_LIMIT})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        "TEST_HTTP_STRESS_CONCURRENT_LIMIT=($ENV{TEST_HTTP_STRESS_CONCURRENT_LIMIT})"
    )
endif()
