/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>

#include "brookesia/hal_adaptor/macro_configs.h"
#include "brookesia/hal_interface/interfaces/storage/file_system.hpp"
#if BROOKESIA_HAL_ADAPTOR_STORAGE_FILE_SYSTEM_ENABLE_FATFS_FLASH
#include "wear_levelling.h"
#endif

namespace esp_brookesia::hal {

class StorageFileSystemImpl: public storage::FileSystemIface {
public:
    StorageFileSystemImpl();
    ~StorageFileSystemImpl();

    bool get_capacity(const char *mount_point, Capacity &capacity) override;

private:
    struct Entry {
        Info info;
        const char *partition_label = nullptr;
    };

    void add_entry(Info info, const char *partition_label);

    bool init_spiffs();
    void deinit_spiffs();

    bool init_littlefs();
    void deinit_littlefs();

    bool init_fatfs_flash();
    void deinit_fatfs_flash();

    bool init_sdcard();
    void deinit_sdcard();

    std::vector<Entry> entries_;

#if BROOKESIA_HAL_ADAPTOR_STORAGE_FILE_SYSTEM_ENABLE_FATFS_FLASH
    wl_handle_t fatfs_flash_wl_handle_ = WL_INVALID_HANDLE;
#endif
};

} // namespace esp_brookesia::hal
