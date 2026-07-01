#
# ESP Platform
#
# Always declare mbedtls for release verify on ESP-IDF (see package_release_verify.cpp).
set(_brookesia_system_core_esp_requires ${COMPONENT_REQUIRES})
list(APPEND _brookesia_system_core_esp_requires mbedtls)

idf_component_register(
    SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    REQUIRES ${_brookesia_system_core_esp_requires}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)

if(CONFIG_BROOKESIA_SYSTEM_CORE_ENABLE_PACKAGE_RELEASE_VERIFY)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BROOKESIA_SYSTEM_CORE_HAVE_PACKAGE_RELEASE_VERIFY=1)
endif()

if(CONFIG_BROOKESIA_SYSTEM_CORE_ENABLE_PACKAGE_PAYLOAD_ENCRYPTION)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BROOKESIA_SYSTEM_CORE_HAVE_PACKAGE_PAYLOAD_ENCRYPTION=1)
endif()

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})
