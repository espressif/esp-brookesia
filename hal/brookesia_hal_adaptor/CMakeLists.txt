set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_INCLUDE_DIRS ${COMPONENT_DIR}/include)
set(COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_DIR}/src)
set(COMPONENT_EMBED_TXTFILES "")
set(COMPONENT_SRCS_CPP "")
set(COMPONENT_PRIV_REQUIRES
    esp_event
    esp_http_server
    esp_netif
    esp_system
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
# is already applied it is skipped. Pass REQUIRED as the fourth argument when
# building without the patch would produce a firmware with incorrect behavior.
function(brookesia_hal_adaptor_apply_component_patch component_name patch_file target_relative_file)
    set(_patch_required FALSE)
    if(ARGC GREATER 3 AND "${ARGV3}" STREQUAL "REQUIRED")
        set(_patch_required TRUE)
    endif()

    set(_patch_failure_level WARNING)
    if(_patch_required)
        set(_patch_failure_level FATAL_ERROR)
    endif()

    if(NOT EXISTS "${patch_file}")
        message(${_patch_failure_level} "brookesia_hal_adaptor: patch file not found: ${patch_file}")
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
        if(_patch_required)
            message(FATAL_ERROR "brookesia_hal_adaptor: cannot resolve required patch component: ${component_name}")
        endif()
        return()
    endif()

    if(NOT EXISTS "${_component_dir}/${target_relative_file}")
        message(${_patch_failure_level}
            "brookesia_hal_adaptor: patch target not found: ${_component_dir}/${target_relative_file}")
        return()
    endif()

    if(NOT GIT_FOUND)
        message(${_patch_failure_level} "brookesia_hal_adaptor: git not found, cannot patch ${component_name}")
        return()
    endif()

    # Already applied? git apply --reverse --check succeeds only on a patched tree.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply --reverse --check --ignore-space-change -p1 "${patch_file}"
        WORKING_DIRECTORY "${_component_dir}"
        RESULT_VARIABLE _reverse_check
        OUTPUT_VARIABLE _reverse_out
        ERROR_VARIABLE _reverse_err
    )
    if(_reverse_check EQUAL 0)
        message(STATUS "brookesia_hal_adaptor: patch already applied to ${component_name}")
        return()
    endif()

    # Can the patch be applied cleanly to the current sources?
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply --check --ignore-space-change -p1 "${patch_file}"
        WORKING_DIRECTORY "${_component_dir}"
        RESULT_VARIABLE _apply_check
        OUTPUT_VARIABLE _apply_out
        ERROR_VARIABLE _apply_err
    )
    if(NOT _apply_check EQUAL 0)
        message(${_patch_failure_level} "brookesia_hal_adaptor: patch does not apply to ${component_name} "
            "(possibly an incompatible version): ${patch_file}")
        return()
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply --ignore-space-change -p1 "${patch_file}"
        WORKING_DIRECTORY "${_component_dir}"
        RESULT_VARIABLE _apply_result
        OUTPUT_QUIET ERROR_QUIET
    )
    if(_apply_result EQUAL 0)
        message(STATUS "brookesia_hal_adaptor: applied patch to ${component_name}: ${patch_file}")
    else()
        message(${_patch_failure_level}
            "brookesia_hal_adaptor: failed to apply patch to ${component_name}: ${patch_file}")
    endif()
endfunction()

set(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_NVS_FLASH FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_LITTLEFS FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_SPIFFS FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_FATFS FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_ESP_BLOCKDEV FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_WEAR_LEVELLING FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_WIFI_STACK FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_BLE_STACK FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_BT_STACK FALSE)
set(BROOKESIA_HAL_ADAPTOR_NEEDS_BT_AUDIO FALSE)
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
    if(CONFIG_BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_RESTART_IMPL)
        brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/system/restart_impl.cpp)
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
        if(CONFIG_BROOKESIA_HAL_ADAPTOR_STORAGE_FILE_SYSTEM_ENABLE_FATFS_NAND)
            set(BROOKESIA_HAL_ADAPTOR_NEEDS_FATFS TRUE)
            set(BROOKESIA_HAL_ADAPTOR_NEEDS_ESP_BLOCKDEV TRUE)
            set(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER TRUE)
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

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_BLE_DEVICE)
    file(GLOB_RECURSE BLE_SRCS_CPP ${COMPONENT_DIR}/src/bluetooth/ble/*.cpp)
    brookesia_hal_adaptor_append_sources(${BLE_SRCS_CPP})
    set(BROOKESIA_HAL_ADAPTOR_NEEDS_BLE_STACK TRUE)
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_BT_DEVICE)
    brookesia_hal_adaptor_append_sources(
        ${COMPONENT_DIR}/src/bluetooth/device.cpp
        ${COMPONENT_DIR}/src/bluetooth/a2dp_sink.cpp
    )
    set(BROOKESIA_HAL_ADAPTOR_NEEDS_BT_STACK TRUE)
    if(CONFIG_BT_CONTROLLER_ENABLED AND CONFIG_BT_CLASSIC_ENABLED AND CONFIG_BT_BLUEDROID_ENABLED AND CONFIG_BT_A2DP_ENABLE AND
       CONFIG_ESP_BT_AUDIO_GMF_IO_SUPPORT)
        set(BROOKESIA_HAL_ADAPTOR_NEEDS_BT_AUDIO TRUE)
    endif()
endif()

# The coordinator is shared by the BLE GATT and Classic Bluetooth profiles.  Keep
# it in the adaptor whenever either profile is enabled so a target-specific A2DP
# implementation can be built without also enabling BLE.
if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_BLE_DEVICE OR
   CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_BT_DEVICE)
    brookesia_hal_adaptor_append_sources(${COMPONENT_DIR}/src/bluetooth/host_coordinator.cpp)
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

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_VIDEO_DEVICE AND
   CONFIG_BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL)
    # av_processor 0.6.x only considers stream mode "running", so its public
    # stop API otherwise leaves a frame-mode capture pipeline open. Keep the
    # workaround local until the dependency release contains the fix.
    set(BROOKESIA_HAL_ADAPTOR_AV_PROCESSOR_PATCH
        ${COMPONENT_DIR}/tools/av_processor_frame_mode_stop.patch
    )
    brookesia_hal_adaptor_apply_component_patch(av_processor
        ${BROOKESIA_HAL_ADAPTOR_AV_PROCESSOR_PATCH}
        src/video_processor.c
        REQUIRED
    )
    brookesia_hal_adaptor_apply_component_patch(jason-mao__av_processor
        ${BROOKESIA_HAL_ADAPTOR_AV_PROCESSOR_PATCH}
        src/video_processor.c
        REQUIRED
    )
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES)
    # Board Manager 0.5.x does not serialize recursive device dependency
    # lifecycle operations. Expansion scanning and camera clients can otherwise
    # race while updating shared handles and reference counts.
    set(BROOKESIA_HAL_ADAPTOR_BOARD_MANAGER_DEVICE_LOCK_PATCH
        ${COMPONENT_DIR}/tools/esp_board_manager_device_lifecycle_lock.patch
    )
    brookesia_hal_adaptor_apply_component_patch(esp_board_manager
        ${BROOKESIA_HAL_ADAPTOR_BOARD_MANAGER_DEVICE_LOCK_PATCH}
        src/esp_board_device.c
        REQUIRED
    )
    brookesia_hal_adaptor_apply_component_patch(espressif__esp_board_manager
        ${BROOKESIA_HAL_ADAPTOR_BOARD_MANAGER_DEVICE_LOCK_PATCH}
        src/esp_board_device.c
        REQUIRED
    )

    # The same release path loses its live peripheral reference when a
    # low-level deinitializer fails. Expansion providers need that reference
    # for retryable reverse-order cleanup.
    set(BROOKESIA_HAL_ADAPTOR_BOARD_MANAGER_PERIPH_PATCH
        ${COMPONENT_DIR}/tools/esp_board_manager_periph_deinit_retry.patch
    )
    brookesia_hal_adaptor_apply_component_patch(esp_board_manager
        ${BROOKESIA_HAL_ADAPTOR_BOARD_MANAGER_PERIPH_PATCH}
        src/esp_board_periph.c
        REQUIRED
    )
    brookesia_hal_adaptor_apply_component_patch(espressif__esp_board_manager
        ${BROOKESIA_HAL_ADAPTOR_BOARD_MANAGER_PERIPH_PATCH}
        src/esp_board_periph.c
        REQUIRED
    )
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_VIDEO_DEVICE AND
   CONFIG_BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL)
    # Make the esp_video 2.4.x DVP teardown staged and retryable, including the
    # open-file check that must run before SCCB and sensor destruction.
    set(BROOKESIA_HAL_ADAPTOR_ESP_VIDEO_DVP_PATCH
        ${COMPONENT_DIR}/tools/esp_video_dvp_deinit_order.patch
    )
    brookesia_hal_adaptor_apply_component_patch(esp_video
        ${BROOKESIA_HAL_ADAPTOR_ESP_VIDEO_DVP_PATCH}
        src/esp_video_init.c
        REQUIRED
    )
    brookesia_hal_adaptor_apply_component_patch(espressif__esp_video
        ${BROOKESIA_HAL_ADAPTOR_ESP_VIDEO_DVP_PATCH}
        src/esp_video_init.c
        REQUIRED
    )
    set(BROOKESIA_HAL_ADAPTOR_ESP_VIDEO_DVP_DETECT_REQUIRED_PATCH
        ${COMPONENT_DIR}/tools/esp_video_dvp_detect_required.patch
    )
    brookesia_hal_adaptor_apply_component_patch(esp_video
        ${BROOKESIA_HAL_ADAPTOR_ESP_VIDEO_DVP_DETECT_REQUIRED_PATCH}
        src/esp_video_init.c
        REQUIRED
    )
    brookesia_hal_adaptor_apply_component_patch(espressif__esp_video
        ${BROOKESIA_HAL_ADAPTOR_ESP_VIDEO_DVP_DETECT_REQUIRED_PATCH}
        src/esp_video_init.c
        REQUIRED
    )
    # esp_capture 0.8.4 only recognises YUYV for YUV422, so a DVP sensor that
    # exposes UYVY alone (the SC101IOT CameraBoard variant) fails negotiation.
    set(BROOKESIA_HAL_ADAPTOR_ESP_CAPTURE_UYVY_PATCH
        ${COMPONENT_DIR}/tools/esp_capture_v4l2_uyvy_support.patch
    )
    brookesia_hal_adaptor_apply_component_patch(esp_capture
        ${BROOKESIA_HAL_ADAPTOR_ESP_CAPTURE_UYVY_PATCH}
        impl/capture_video_src/capture_video_v4l2_src.c
        REQUIRED
    )
    brookesia_hal_adaptor_apply_component_patch(espressif__esp_capture
        ${BROOKESIA_HAL_ADAPTOR_ESP_CAPTURE_UYVY_PATCH}
        impl/capture_video_src/capture_video_v4l2_src.c
        REQUIRED
    )

    # esp_board_manager 0.5.x hides DVP camera shutdown failures and releases
    # resources that are still in use. Mosaico needs a retryable close before
    # restoring the expansion-slot address pins.
    set(BROOKESIA_HAL_ADAPTOR_BOARD_MANAGER_CAMERA_PATCH
        ${COMPONENT_DIR}/tools/esp_board_manager_dvp_camera_deinit.patch
    )
    brookesia_hal_adaptor_apply_component_patch(esp_board_manager
        ${BROOKESIA_HAL_ADAPTOR_BOARD_MANAGER_CAMERA_PATCH}
        devices/dev_camera/dev_camera.c
        REQUIRED
    )
    brookesia_hal_adaptor_apply_component_patch(espressif__esp_board_manager
        ${BROOKESIA_HAL_ADAPTOR_BOARD_MANAGER_CAMERA_PATCH}
        devices/dev_camera/dev_camera.c
        REQUIRED
    )
endif()

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
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_ESP_BLOCKDEV esp_blockdev)
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
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_BLE_STACK bt)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_BT_STACK bt)
brookesia_hal_adaptor_optional_private_requires(BROOKESIA_HAL_ADAPTOR_NEEDS_BT_AUDIO
    esp_bt_audio
    espressif__esp_bt_audio
    gmf_audio
    espressif__gmf_audio
    gmf_io
    espressif__gmf_io
    gmf_core
    espressif__gmf_core
    esp_audio_codec
    espressif__esp_audio_codec
    esp_codec_dev
    espressif__esp_codec_dev
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
if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_BLE_DEVICE)
    if(CONFIG_BT_CONTROLLER_DISABLED)
        idf_component_optional_requires(PRIVATE esp_hosted espressif__esp_hosted)
    endif()
endif()

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_BT_DEVICE)
    set(BROOKESIA_HAL_ADAPTOR_BT_DEVICE_PLUGIN_SYMBOL bluetooth_device_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_HAL_ADAPTOR_BT_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_HAL_ADAPTOR_BT_DEVICE_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_HAL_ADAPTOR_BT_DEVICE_PLUGIN_SYMBOL}")
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

if(CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_BLE_DEVICE)
    set(BROOKESIA_HAL_ADAPTOR_BLE_DEVICE_PLUGIN_SYMBOL ble_device_symbol)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BROOKESIA_HAL_ADAPTOR_BLE_DEVICE_PLUGIN_SYMBOL=${BROOKESIA_HAL_ADAPTOR_BLE_DEVICE_PLUGIN_SYMBOL}
    )
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_HAL_ADAPTOR_BLE_DEVICE_PLUGIN_SYMBOL}")
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
