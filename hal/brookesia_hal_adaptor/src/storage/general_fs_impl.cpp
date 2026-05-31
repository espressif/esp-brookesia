/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include <thread>
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "general_fs_impl.hpp"
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SPIFFS || \
    BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SDCARD
#include "esp_board_manager_includes.h"
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SPIFFS
#include "esp_spiffs.h"
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_LITTLEFS
#include "esp_littlefs.h"
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_FATFS_FLASH || \
    BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SDCARD
#include "esp_vfs_fat.h"
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SDCARD
#include "dev_fs_fat.h"
#endif

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_GENERAL_FS_IMPL

namespace esp_brookesia::hal {

GeneralStorageFsImpl::GeneralStorageFsImpl()
{
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SPIFFS
    BROOKESIA_CHECK_FALSE_EXECUTE(init_spiffs(), {}, { BROOKESIA_LOGE("Failed to initialize SPIFFS"); });
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_LITTLEFS
    BROOKESIA_CHECK_FALSE_EXECUTE(init_littlefs(), {}, { BROOKESIA_LOGE("Failed to initialize LittleFS"); });
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_FATFS_FLASH
    BROOKESIA_CHECK_FALSE_EXECUTE(init_fatfs_flash(), {}, { BROOKESIA_LOGE("Failed to initialize flash FATFS"); });
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
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_LITTLEFS
    deinit_littlefs();
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_FATFS_FLASH
    deinit_fatfs_flash();
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SDCARD
    deinit_sdcard();
#endif
}

bool GeneralStorageFsImpl::get_capacity(const char *mount_point, Capacity &capacity)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_CHECK_NULL_RETURN(mount_point, false, "Invalid mount point");

    for (const auto &entry : entries_) {
        if (std::strcmp(entry.info.mount_point, mount_point) != 0) {
            continue;
        }

        switch (entry.info.fs_type) {
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SPIFFS
        case FileSystemType::SPIFFS: {
            size_t total_size = 0;
            size_t used_size = 0;
            auto ret = esp_spiffs_info(entry.partition_label, &total_size, &used_size);
            BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get SPIFFS capacity");
            capacity = {
                .total_bytes = total_size,
                .used_bytes = used_size,
                .free_bytes = total_size - used_size,
            };
            return true;
        }
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_LITTLEFS
        case FileSystemType::LittleFS: {
            size_t total_size = 0;
            size_t used_size = 0;
            auto ret = esp_littlefs_info(entry.partition_label, &total_size, &used_size);
            BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get LittleFS capacity");
            capacity = {
                .total_bytes = total_size,
                .used_bytes = used_size,
                .free_bytes = total_size - used_size,
            };
            return true;
        }
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_FATFS_FLASH || \
BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SDCARD
        case FileSystemType::FATFS: {
            uint64_t total_size = 0;
            uint64_t free_size = 0;
            auto ret = esp_vfs_fat_info(entry.info.mount_point, &total_size, &free_size);
            BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get FATFS capacity");
            capacity = {
                .total_bytes = total_size,
                .used_bytes = total_size - free_size,
                .free_bytes = free_size,
            };
            return true;
        }
#endif
        default:
            return false;
        }
    }

    BROOKESIA_LOGW("File system not found for mount point: %1%", mount_point);
    return false;
}

void GeneralStorageFsImpl::add_entry(Info info, const char *partition_label)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    entries_.push_back(Entry{
        .info = info,
        .partition_label = partition_label,
    });
    info_list_.push_back(entries_.back().info);
}

#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SPIFFS
bool GeneralStorageFsImpl::init_spiffs()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    esp_err_t ret = ESP_OK;
    auto init_func = [&ret]() {
        BROOKESIA_LOG_TRACE_GUARD();
        ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SPIFFS);
    };
    if (!lib_utils::ThreadConfig::check_stack_cache_safe()) {
        // Since initializing SPIFFS operates on Flash,
        // a separate thread with its stack located in SRAM needs to be created to prevent a crash.
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        std::thread(init_func).join();
    } else {
        init_func();
    }
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

    add_entry(Info {
        .fs_type = FileSystemType::SPIFFS,
        .medium_type = MediumType::Flash,
        .mount_point = cfg->base_path,
        .supports_directories = false,
    }, cfg->partition_label);

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

#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_LITTLEFS
bool GeneralStorageFsImpl::init_littlefs()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    esp_err_t ret = ESP_OK;
    esp_vfs_littlefs_conf_t config = {};
    config.base_path = BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_LITTLEFS_BASE_PATH;
    config.partition_label = BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_LITTLEFS_PARTITION_LABEL;
    config.format_if_mount_failed = BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_LITTLEFS_FORMAT_IF_MOUNT_FAILED;
    config.dont_mount = false;
    auto init_func = [&ret, &config]() {
        BROOKESIA_LOG_TRACE_GUARD();
        ret = esp_vfs_littlefs_register(&config);
    };
    if (!lib_utils::ThreadConfig::check_stack_cache_safe()) {
        // Since initializing LittleFS operates on Flash,
        // a separate thread with its stack located in SRAM needs to be created to prevent a crash.
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        std::thread(init_func).join();
    } else {
        init_func();
    }
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to initialize LittleFS");
    lib_utils::FunctionGuard deinit_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        esp_vfs_littlefs_unregister(BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_LITTLEFS_PARTITION_LABEL);
    });

    size_t total_size = 0;
    size_t used_size = 0;
    ret = esp_littlefs_info(BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_LITTLEFS_PARTITION_LABEL, &total_size, &used_size);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get LittleFS partition information");
    BROOKESIA_LOGI("LittleFS partition size: total: %1%, used: %2%", total_size, used_size);

    add_entry(Info {
        .fs_type = FileSystemType::LittleFS,
        .medium_type = MediumType::Flash,
        .mount_point = BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_LITTLEFS_BASE_PATH,
        .supports_directories = true,
    }, BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_LITTLEFS_PARTITION_LABEL);

    deinit_guard.release();

    return true;
}

void GeneralStorageFsImpl::deinit_littlefs()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_vfs_littlefs_unregister(BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_LITTLEFS_PARTITION_LABEL);
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinitialize LittleFS"); });
}
#endif

#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_FATFS_FLASH
bool GeneralStorageFsImpl::init_fatfs_flash()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    esp_err_t ret = ESP_OK;
    esp_vfs_fat_mount_config_t config = {};
    config.max_files = BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_FATFS_FLASH_MAX_FILES;
    config.format_if_mount_failed = BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_FATFS_FLASH_FORMAT_IF_MOUNT_FAILED;
    config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;
    config.use_one_fat = false;
    auto init_func = [&ret, &config, this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        ret = esp_vfs_fat_spiflash_mount_rw_wl(
                  BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_FATFS_FLASH_BASE_PATH,
                  BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_FATFS_FLASH_PARTITION_LABEL,
                  &config,
                  &fatfs_flash_wl_handle_
              );
    };
    if (!lib_utils::ThreadConfig::check_stack_cache_safe()) {
        // Since initializing flash FATFS operates on Flash, use an SRAM stack when needed.
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        std::thread(init_func).join();
    } else {
        init_func();
    }
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to initialize flash FATFS");
    lib_utils::FunctionGuard deinit_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (fatfs_flash_wl_handle_ != WL_INVALID_HANDLE) {
            esp_vfs_fat_spiflash_unmount_rw_wl(
                BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_FATFS_FLASH_BASE_PATH, fatfs_flash_wl_handle_
            );
            fatfs_flash_wl_handle_ = WL_INVALID_HANDLE;
        }
    });

    uint64_t total_size = 0;
    uint64_t free_size = 0;
    ret = esp_vfs_fat_info(
              BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_FATFS_FLASH_BASE_PATH, &total_size, &free_size
          );
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get flash FATFS partition information");
    BROOKESIA_LOGI(
        "Flash FATFS partition size: total: %1%, used: %2%, free: %3%",
        total_size, total_size - free_size, free_size
    );

    add_entry(Info {
        .fs_type = FileSystemType::FATFS,
        .medium_type = MediumType::Flash,
        .mount_point = BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_FATFS_FLASH_BASE_PATH,
        .supports_directories = true,
    }, BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_FATFS_FLASH_PARTITION_LABEL);

    deinit_guard.release();

    return true;
}

void GeneralStorageFsImpl::deinit_fatfs_flash()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (fatfs_flash_wl_handle_ == WL_INVALID_HANDLE) {
        return;
    }

    auto ret = esp_vfs_fat_spiflash_unmount_rw_wl(
                   BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_FATFS_FLASH_BASE_PATH, fatfs_flash_wl_handle_
               );
    fatfs_flash_wl_handle_ = WL_INVALID_HANDLE;
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinitialize flash FATFS"); });
}
#endif

#if BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SDCARD
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

    add_entry(Info {
        .fs_type = FileSystemType::FATFS,
        .medium_type = MediumType::SDCard,
        .mount_point = cfg->mount_point,
        .supports_directories = true,
    }, "");

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
