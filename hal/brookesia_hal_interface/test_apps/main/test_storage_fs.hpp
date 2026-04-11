/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/storage/fs.hpp"

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
            },
            hal::StorageFsIface::Info{
                .fs_type = hal::StorageFsIface::FileSystemType::LittleFS,
                .medium_type = hal::StorageFsIface::MediumType::Flash,
                .mount_point = "/littlefs",
            },
        };
    }
    ~TestStorageFsIface() = default;
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
