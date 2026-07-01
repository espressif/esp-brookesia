idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity esp_psram esp_wifi
    WHOLE_ARCHIVE TRUE
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    TEST_SNTP_ENABLE_NETWORK=1
    TEST_SNTP_USE_WIFI=1
)

if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP1_SSID})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        TEST_SNTP_WIFI_SSID="$ENV{CI_TEST_WIFI_2_4G_AP1_SSID}"
    )
endif()

if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP1_PSW})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        TEST_SNTP_WIFI_PASSWORD="$ENV{CI_TEST_WIFI_2_4G_AP1_PSW}"
    )
endif()

if(DEFINED ENV{TEST_SNTP_NTP_SERVER})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        TEST_SNTP_NTP_SERVER="$ENV{TEST_SNTP_NTP_SERVER}"
    )
endif()

if(DEFINED ENV{TEST_SNTP_TIMEZONE})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        TEST_SNTP_TIMEZONE="$ENV{TEST_SNTP_TIMEZONE}"
    )
endif()

if(DEFINED ENV{TEST_SNTP_SYNC_TIMEOUT_MS})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        "TEST_SNTP_SYNC_TIMEOUT_MS=($ENV{TEST_SNTP_SYNC_TIMEOUT_MS})"
    )
endif()

if(DEFINED ENV{TEST_SNTP_MIN_VALID_UNIX_TIME})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        "TEST_SNTP_MIN_VALID_UNIX_TIME=($ENV{TEST_SNTP_MIN_VALID_UNIX_TIME})"
    )
endif()
