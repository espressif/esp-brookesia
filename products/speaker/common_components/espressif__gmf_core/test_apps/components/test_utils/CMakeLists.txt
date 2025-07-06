set(srcs "memory_checks.c"
         "test_runner.c"
         "test_utils.c")

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS include
                    REQUIRES esp_partition cmock newlib
                    PRIV_REQUIRES perfmon esp_netif)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
