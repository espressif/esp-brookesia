/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <string>

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/storage/file_system.hpp"

namespace esp_brookesia {

class TestStorageFsIface: public hal::storage::FileSystemIface {
public:
    static constexpr const char *NAME = "TestStorageFs:StorageFs";

    TestStorageFsIface()
        : hal::storage::FileSystemIface()
    {
        info_list_ = {
            hal::storage::FileSystemIface::Info{
                .fs_type = hal::storage::FileSystemIface::FileSystemType::SPIFFS,
                .medium_type = hal::storage::FileSystemIface::MediumType::Flash,
                .mount_point = "/spiffs",
                .root_path = "/spiffs",
                .supports_directories = false,
            },
            hal::storage::FileSystemIface::Info{
                .fs_type = hal::storage::FileSystemIface::FileSystemType::LittleFS,
                .medium_type = hal::storage::FileSystemIface::MediumType::Flash,
                .mount_point = "/littlefs",
                .root_path = "/littlefs",
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
    std::vector<hal::InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia
