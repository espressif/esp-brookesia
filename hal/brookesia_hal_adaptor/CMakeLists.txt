set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_INCLUDE_DIRS ${COMPONENT_DIR}/include)
set(COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_DIR}/src)
set(COMPONENT_EMBED_TXTFILES "")
set(COMPONENT_SRCS_CPP "")
set(COMPONENT_PRIV_REQUIRES
    esp_event
    esp_http_server
    esp_netif
    esp_timer
    esp_wifi
    lwip
)

macro(brookesia_hal_adaptor_append_sources)
    list(APPEND COMPONENT_SRCS_CPP ${ARGN})
endmacro()

macro(brookesia_hal_adaptor_optional_private_requires condition)
    if(${condition})
        idf_component_optional_requires(PRIVATE ${ARGN})
    endif()
endmacro()

# Apply a patch shipped with this component to a resolved dependency component.
#
# The patch is applied during the CMake configure phase and is idempotent: if it
# is already applied it is skipped, and if it cannot be applied (e.g. an
# incompatible upstream version) a warning is emitted instead of failing the
# whole build.
function(brookesia_hal_adaptor_apply_component_patch component_name patch_file target_relative_file)
    if(NOT EXISTS "${patch_file}")
        message(WARNING "brookesia_hal_adaptor: patch file not found, skip: ${patch_file}")
        return()
    endif()

    # BUILD_COMPONENTS is populated during requirement expansion, before any
    # component's CMakeLists.txt runs, so this membership check is independent of
    # component processing order (unlike a "TARGET __idf_<name>" check). It also
    # guarantees idf_component_get_property() can resolve the component instead of
    # raising a FATAL_ERROR for an unknown component.
    idf_build_get_property(_build_components BUILD_COMPONENTS)
    if(NOT "${component_name}" IN_LIST _build_components)
        return()
    endif()

    idf_component_get_property(_component_dir ${component_name} COMPONENT_DIR)
    if(NOT _component_dir OR NOT EXISTS "${_component_dir}")
        return()
    endif()

    if(NOT EXISTS "${_component_dir}/${target_relative_file}")
        message(WARNING "brookesia_hal_adaptor: patch target not found, skip: ${_component_dir}/${target_relative_file}")
        return()
    endif()

    if(NOT GIT_FOUND)
        message(WARNING "brookesia_hal_adaptor: git not found, cannot patch ${component_name}")
        return()
    endif()

    # Already applied? git apply --reverse --check succeeds only on a patched tree.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply --reverse --check -p1 "${patch_file}"
        WORKING_DIRECTORY "${_component_dir}"
        RESULT_VARIABLE _reverse_check
        OUTPUT_QUIET ERROR_QUIET
    )
    if(_reverse_check EQUAL 0)
        message(STATUS "brookesia_hal_adaptor: patch already applied to ${component_name}")
        return()
    endif()

    # Can the patch be applied cleanly to the current sources?
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply --check -p1 "${patch_file}"
        WORKING_DIRECTORY "${_component_dir}"
        RESULT_VARIABLE _apply_check
        OUTPUT_QUIET ERROR_QUIET
    )
    if(NOT _apply_check EQUAL 0)
        message(WARNING "brookesia_hal_adaptor: patch does not apply to ${component_name} "
            "(possibly an incompatible version), skipping: ${patch_file}")
        return()
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply -p1 "${patch_file}"
        WORKING_DIRECTORY "${_component_dir}"
        RESULT_VARIABLE _apply_result
        OUTPUT_QUIET ERROR_QUIET
    )
    if(_apply_result EQUAL 0)
        message(STATUS "brookesia_hal_adaptor: applied patch to ${component_name}: ${patch_file}")
    else()
        message(WARNING "brookesia_hal_adaptor: failed to apply patch to ${component_name}: ${patch_file}")
    endif()
endfunction()

set(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_NVS_FLASH FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_LITTLEFS FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_SPIFFS FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_FATFS FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_WEAR_LEVELLING FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_WIFI_STACK FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_SNTP_STACK FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_HTTP_STACK FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_APP_UPDATE FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_AV_PROCESSOR FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_ESP_VIDEO FALSE)

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_SYSTEM_DEVICE)
    brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/system/device.cpp)
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_BOARD_INFO_IMPL)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/system/board_info_impl.cpp)
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER TRUE)
    endif()
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_OTA_UPDATER_IMPL)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/system/ota_updater_impl.cpp)
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_APP_UPDATE TRUE)
        if(CONFIG_ESP_BOARD_NAME)
            set(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER TRUE)
        endif()
    endif()
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_NETWORK_DEVICE)
    brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/network/device.cpp)
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_SNTP_CLIENT_IMPL)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/network/sntp_client_impl.cpp)
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_SNTP_STACK TRUE)
    endif()
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_HTTP_CLIENT_IMPL)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/network/http_client_impl.cpp)
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_HTTP_STACK TRUE)
    endif()
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_AUDIO_DEVICE)
    brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/audio/device.cpp)
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/audio/codec_player_impl.cpp)
    endif()
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_RECORDER_IMPL)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/audio/codec_recorder_impl.cpp)
    endif()
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_IMPL)
        brookesia_hal_adaptor_append_sources(
            ${COMPONENT_DIR}/src/audio/processor_impl.cpp
            ${COMPONENT_DIR}/src/audio/processor_type_converter.cpp
        )
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_AV_PROCESSOR TRUE)
    endif()
    set(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER TRUE)
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_VIDEO_DEVICE)
    brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/video/device.cpp)
    set(BROOKESIA_HAL_ADAPTOR_NEEDS_VIDEO_CONVERTER FALSE)
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/video/camera_impl.cpp)
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER TRUE)
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_ESP_VIDEO TRUE)
    endif()
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/video/encoder_impl.cpp)
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_VIDEO_CONVERTER TRUE)
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_AV_PROCESSOR TRUE)
    endif()
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_DECODER_IMPL)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/video/decoder_impl.cpp)
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_VIDEO_CONVERTER TRUE)
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_AV_PROCESSOR TRUE)
    endif()
    if(BROOKESIA_HAL_ADAPTOR_NEEDS_VIDEO_CONVERTER)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/video/processor_type_converter.cpp)
    endif()
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_DISPLAY_DEVICE)
    file(GLOB_RECURSE DISPLAY_SRCS_CPP ${COMPONENT_DIR}/src/display/*.cpp)
    brookesia_hal_adaptor_append_sources(${DISPLAY_SRCS_CPP})
    set(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER TRUE)
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_STORAGE_DEVICE)
    brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/storage/device.cpp)
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_FILE_SYSTEM_IMPL)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/storage/file_system_impl.cpp)

        if(CONFIG_BROOKESIA_HAL_ADAPTOR_STORAGE_FILE_SYSTEM_ENABLE_SPIFFS)
            set(BROOKESIA_HAL_ADAPTOR_NEEDS_SPIFFS TRUE)
            set(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER TRUE)
        endif()
        if(CONFIG_BROOKESIA_HAL_ADAPTOR_STORAGE_FILE_SYSTEM_ENABLE_LITTLEFS)
            set(BROOKESIA_HAL_ADAPTOR_NEEDS_LITTLEFS TRUE)
        endif()
        if(CONFIG_BROOKESIA_HAL_ADAPTOR_STORAGE_FILE_SYSTEM_ENABLE_FATFS_FLASH)
            set(BROOKESIA_HAL_ADAPTOR_NEEDS_FATFS TRUE)
            set(BROOKESIA_HAL_ADAPTOR_NEEDS_WEAR_LEVELLING TRUE)
        endif()
        if(CONFIG_BROOKESIA_HAL_ADAPTOR_STORAGE_FILE_SYSTEM_ENABLE_SDCARD)
            set(BROOKESIA_HAL_ADAPTOR_NEEDS_FATFS TRUE)
            set(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER TRUE)
        endif()
    endif()
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/storage/key_value_nvs_impl.cpp)
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_NVS_FLASH TRUE)
    endif()
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_POWER_DEVICE)
    file(GLOB_RECURSE POWER_SRCS_CPP ${COMPONENT_DIR}/src/power/*.cpp)
    brookesia_hal_adaptor_append_sources(${POWER_SRCS_CPP})
    set(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER TRUE)
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_WIFI_DEVICE)
    file(GLOB_RECURSE WIFI_SRCS_CPP ${COMPONENT_DIR}/src/wifi/*.cpp)
    brookesia_hal_adaptor_append_sources(${WIFI_SRCS_CPP})
    list(APPEND COMPONENT_EMBED_TXTFILES
        ${COMPONENT_DIR}/src/wifi/wifi_provision.html
    )
    set(BROOKESIA_HAL_ADAPTOR_NEEDS_WIFI_STACK TRUE)
    set(BROOKESIA_HAL_ADAPTOR_NEEDS_NVS_FLASH TRUE)
endif()

if(BROOKESIA_HAL_ADAPTOR_NEEDS_NVS_FLASH)
    brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/private/nvs_flash_manager.cpp)
endif()

#
# Register Component
#
idf_component_register(
    SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    REQUIRES ${COMPONENT_REQUIRES}
    PRIV_REQUIRES ${COMPONENT_PRIV_REQUIRES}
    EMBED_TXTFILES ${COMPONENT_EMBED_TXTFILES}
)

#
# Patch the resolved media_lib_sal component so its esp_tls usage builds on
# ESP-IDF 6.x. The dependency may be resolved either as a local override
# (media_lib_sal) or a managed component (espressif__media_lib_sal).
#
find_package(Git QUIET)
set(BROOKESIA_HAL_ADAPTOR_MEDIA_LIB_SAL_PATCH
    ${COMPONENT_DIR}/tools/media_lib_sal_esp_tls_idf6.patch
)
brookesia_hal_adaptor_apply_component_patch(media_lib_sal
    ${BROOKESIA_HAL_ADAPTOR_MEDIA_LIB_SAL_PATCH}
    port/media_lib_tls_default.c
)
brookesia_hal_adaptor_apply_component_patch(espressif__media_lib_sal
    ${BROOKESIA_HAL_ADAPTOR_MEDIA_LIB_SAL_PATCH}
    port/media_lib_tls_default.c
)

brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER
    esp_board_manager
    espressif__esp_board_manager
)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_ESP_VIDEO
    esp_video
    espressif__esp_video
)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_SPIFFS spiffs)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_LITTLEFS
    littlefs
    joltwallet__littlefs
)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_FATFS fatfs)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_WEAR_LEVELLING wear_levelling)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_NVS_FLASH nvs_flash)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_WIFI_STACK
    esp_event
    esp_http_server
    esp_netif
    esp_timer
    esp_wifi
    lwip
)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_SNTP_STACK
    esp_event
    esp_netif
    lwip
)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_HTTP_STACK
    esp_http_client
    esp-tls
    mbedtls
)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_APP_UPDATE app_update)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_AV_PROCESSOR
    av_processor
    jason-mao__av_processor
)
if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_WIFI_DEVICE)
    if(CONFIG_IDF_TARGET_ESP32P4)
        idf_component_optional_requires(PRIVATE
            esp_wifi_remote espressif__esp_wifi_remote
            esp_hosted espressif__esp_hosted
        )
    endif()
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_SYSTEM_DEVICE)
    set(BROOKESIA_HAL_ADAPTOR_SYSTEM_DEVICE_PLUGIN_SYMBOL system_device_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_HAL_ADAPTOR_SYSTEM_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_HAL_ADAPTOR_SYSTEM_DEVICE_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_HAL_ADAPTOR_SYSTEM_DEVICE_PLUGIN_SYMBOL}")
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_NETWORK_DEVICE)
    set(BROOKESIA_HAL_ADAPTOR_NETWORK_DEVICE_PLUGIN_SYMBOL network_device_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_HAL_ADAPTOR_NETWORK_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_HAL_ADAPTOR_NETWORK_DEVICE_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_HAL_ADAPTOR_NETWORK_DEVICE_PLUGIN_SYMBOL}")
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_AUDIO_DEVICE)
    set(BROOKESIA_HAL_ADAPTOR_AUDIO_DEVICE_PLUGIN_SYMBOL audio_codec_device_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_HAL_ADAPTOR_AUDIO_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_HAL_ADAPTOR_AUDIO_DEVICE_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_HAL_ADAPTOR_AUDIO_DEVICE_PLUGIN_SYMBOL}")
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_WIFI_DEVICE)
    set(BROOKESIA_HAL_ADAPTOR_WIFI_DEVICE_PLUGIN_SYMBOL wifi_device_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_HAL_ADAPTOR_WIFI_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_HAL_ADAPTOR_WIFI_DEVICE_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_HAL_ADAPTOR_WIFI_DEVICE_PLUGIN_SYMBOL}")
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_DISPLAY_DEVICE)
    set(BROOKESIA_HAL_ADAPTOR_DISPLAY_DEVICE_PLUGIN_SYMBOL display_lcd_device_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_HAL_ADAPTOR_DISPLAY_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_HAL_ADAPTOR_DISPLAY_DEVICE_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_HAL_ADAPTOR_DISPLAY_DEVICE_PLUGIN_SYMBOL}")
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_STORAGE_DEVICE AND
    (CONFIG_BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_FILE_SYSTEM_IMPL OR
     CONFIG_BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL))
    set(BROOKESIA_HAL_ADAPTOR_STORAGE_DEVICE_PLUGIN_SYMBOL storage_device_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_HAL_ADAPTOR_STORAGE_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_HAL_ADAPTOR_STORAGE_DEVICE_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_HAL_ADAPTOR_STORAGE_DEVICE_PLUGIN_SYMBOL}")
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_VIDEO_DEVICE)
    set(BROOKESIA_HAL_ADAPTOR_VIDEO_DEVICE_PLUGIN_SYMBOL video_device_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_HAL_ADAPTOR_VIDEO_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_HAL_ADAPTOR_VIDEO_DEVICE_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_HAL_ADAPTOR_VIDEO_DEVICE_PLUGIN_SYMBOL}")
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_POWER_DEVICE)
    set(BROOKESIA_HAL_ADAPTOR_POWER_DEVICE_PLUGIN_SYMBOL power_device_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_HAL_ADAPTOR_POWER_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_HAL_ADAPTOR_POWER_DEVICE_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_HAL_ADAPTOR_POWER_DEVICE_PLUGIN_SYMBOL}")
endif()
