idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity esp_psram
    WHOLE_ARCHIVE
)

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)

# Check for CI test WiFi credentials environment variables
if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP1_SSID})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        CI_TEST_WIFI_2_4G_AP1_SSID="$ENV{CI_TEST_WIFI_2_4G_AP1_SSID}"
    )
endif()
if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP1_PSW})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        CI_TEST_WIFI_2_4G_AP1_PSW="$ENV{CI_TEST_WIFI_2_4G_AP1_PSW}"
    )
endif()
if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP2_SSID})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        CI_TEST_WIFI_2_4G_AP2_SSID="$ENV{CI_TEST_WIFI_2_4G_AP2_SSID}"
    )
endif()
if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP2_PSW})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        CI_TEST_WIFI_2_4G_AP2_PSW="$ENV{CI_TEST_WIFI_2_4G_AP2_PSW}"
    )
endif()
