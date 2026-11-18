#
# ESP Platform
#
# Always declare mbedtls for release verify on ESP-IDF (see package_release_verify.cpp).
if(NOT CONFIG_BROOKESIA_GUI_BACKEND_GSP)
    list(REMOVE_ITEM COMPONENT_SRCS_CPP "${COMPONENT_SRC_DIR}/system/gsp_shell_dark.cpp")
endif()
set(_brookesia_system_core_esp_requires ${COMPONENT_REQUIRES})
list(APPEND _brookesia_system_core_esp_requires mbedtls)
if(CONFIG_BROOKESIA_SYSTEM_CORE_ENABLE_USB_BRIDGE)
    list(APPEND _brookesia_system_core_esp_requires brookesia_service_usb)
endif()

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

if(CONFIG_BROOKESIA_GUI_BACKEND_GSP)
    if(NOT COMMAND brookesia_gsp_add_bundle)
        message(FATAL_ERROR "brookesia_gui_gsp bundle helper is unavailable")
    endif()
    gsp_enable_freetype()
    set(super_shell_scenes
        "${COMPONENT_DIR}/gsp/super_launcher_1024.json"
        "${COMPONENT_DIR}/gsp/super_keyboard_1024.json"
        "${COMPONENT_DIR}/gsp/super_dialog_1024.json"
    )
    brookesia_gsp_prepare_theme(super_shell_light_scenes light ${super_shell_scenes})
    brookesia_gsp_prepare_theme(super_shell_dark_scenes dark ${super_shell_scenes})
    brookesia_gsp_add_bundle(${COMPONENT_LIB}
        SCENES ${super_shell_light_scenes}
        PIXEL_FORMAT rgb565
        SYMBOL super_shell
    )
    if(CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES)
        brookesia_gsp_compress_bundle(${COMPONENT_LIB} SYMBOL super_shell)
    endif()
    brookesia_gsp_add_bundle(${COMPONENT_LIB}
        SCENES ${super_shell_dark_scenes}
        PIXEL_FORMAT rgb565
        SYMBOL super_shell_dark
    )
    if(CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES)
        brookesia_gsp_compress_bundle(${COMPONENT_LIB} SYMBOL super_shell_dark)
    endif()
endif()

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})
