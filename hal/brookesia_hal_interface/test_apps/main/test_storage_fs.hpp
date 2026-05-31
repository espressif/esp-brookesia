/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <string>

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/storage/fs.hpp"

namespace esp_brookesia {

class TestStorageFsIface: public hal::StorageFsIface {
public:
    static constexpr const char *NAME = "TestStorageFs:StorageFs";

    TestStorageFsIface()
        : StorageFsIface()
    {
        info_list_ = {
            hal::StorageFsIface::Info{
                .fs_type = hal::StorageFsIface::FileSystemType::SPIFFS,
                .medium_type = hal::StorageFsIface::MediumType::Flash,
                .mount_point = "/spiffs",
                .supports_directories = false,
            },
            hal::StorageFsIface::Info{
                .fs_type = hal::StorageFsIface::FileSystemType::LittleFS,
                .medium_type = hal::StorageFsIface::MediumType::Flash,
                .mount_point = "/littlefs",
                .supports_directories = true,
            },
        };
    }
    ~TestStorageFsIface() = default;

    bool get_capacity(const char *mount_point, Capacity &capacity) override
    {
        if (std::string(mount_point) == "/spiffs") {
            capacity = {
                .total_bytes = 1024 * 1024,
                .used_bytes = 256 * 1024,
                .free_bytes = 768 * 1024,
            };
            return true;
        }
        if (std::string(mount_point) == "/littlefs") {
            capacity = {
                .total_bytes = 2 * 1024 * 1024,
                .used_bytes = 512 * 1024,
                .free_bytes = 1536 * 1024,
            };
            return true;
        }

        return false;
    }
};

class TestStorageFsDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestStorageFs";

    TestStorageFsDevice() : hal::Device(std::string(NAME)) {}

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia
