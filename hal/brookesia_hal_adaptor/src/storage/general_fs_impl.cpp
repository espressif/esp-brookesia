/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_board_manager_includes.h"
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "general_fs_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_GENERAL_FS_IMPL

namespace esp_brookesia::hal {

GeneralStorageFsImpl::GeneralStorageFsImpl()
{
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SPIFFS
    BROOKESIA_CHECK_FALSE_EXECUTE(init_spiffs(), {}, { BROOKESIA_LOGE("Failed to initialize SPIFFS"); });
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SDCARD
    BROOKESIA_CHECK_FALSE_EXECUTE(init_sdcard(), {}, { BROOKESIA_LOGE("Failed to initialize SD card"); });
#endif
}

GeneralStorageFsImpl::~GeneralStorageFsImpl()
{
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SPIFFS
    deinit_spiffs();
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SDCARD
    deinit_sdcard();
#endif
}

#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SPIFFS
bool GeneralStorageFsImpl::init_spiffs()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SPIFFS);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to initialize SPIFFS");
    lib_utils::FunctionGuard deinit_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SPIFFS);
    });

    dev_fs_spiffs_config_t *cfg = NULL;
    ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_FS_SPIFFS, reinterpret_cast<void **>(&cfg));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get SPIFFS configuration");
    BROOKESIA_CHECK_NULL_RETURN(cfg, false, "Failed to get SPIFFS configuration");

    esp_board_device_show(ESP_BOARD_DEVICE_NAME_FS_SPIFFS);

    info_list_.emplace_back(Info {
        .fs_type = FileSystemType::SPIFFS,
        .medium_type = MediumType::Flash,
        .mount_point = cfg->base_path,
    });

    deinit_guard.release();

    return true;
}

void GeneralStorageFsImpl::deinit_spiffs()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SPIFFS);
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinitialize SPIFFS"); });
}
#endif

#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SDCARD
#include "dev_fs_fat.h"

bool GeneralStorageFsImpl::init_sdcard()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to initialize SD card");
    lib_utils::FunctionGuard deinit_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    });

    dev_fs_fat_config_t *cfg = NULL;
    ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_FS_SDCARD, reinterpret_cast<void **>(&cfg));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get SD card configuration");
    BROOKESIA_CHECK_NULL_RETURN(cfg, false, "Failed to get SD card configuration");

    esp_board_device_show(ESP_BOARD_DEVICE_NAME_FS_SDCARD);

    info_list_.emplace_back(Info {
        .fs_type = FileSystemType::FATFS,
        .medium_type = MediumType::SDCard,
        .mount_point = cfg->mount_point,
    });

    deinit_guard.release();

    return true;
}

void GeneralStorageFsImpl::deinit_sdcard()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinitialize SD card"); });
}
#endif

} // namespace esp_brookesia::hal

#endif // BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_GENERAL_FS_IMPL
