/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class StorageKeyValueLinuxImpl;
class StorageFileSystemLinuxImpl;

class StorageLinuxDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "StorageLinux";
    static constexpr const char *KEY_VALUE_IFACE_NAME = "StorageLinux:KeyValue";
    static constexpr const char *FILE_SYSTEM_IFACE_NAME = "StorageLinux:FileSystem";

    StorageLinuxDevice(const StorageLinuxDevice &) = delete;
    StorageLinuxDevice &operator=(const StorageLinuxDevice &) = delete;
    StorageLinuxDevice(StorageLinuxDevice &&) = delete;
    StorageLinuxDevice &operator=(StorageLinuxDevice &&) = delete;

    static StorageLinuxDevice &get_instance()
    {
        static StorageLinuxDevice instance;
        return instance;
    }

private:
    StorageLinuxDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~StorageLinuxDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<StorageKeyValueLinuxImpl> key_value_iface_;
    std::shared_ptr<StorageFileSystemLinuxImpl> file_system_iface_;
};

} // namespace esp_brookesia::hal
