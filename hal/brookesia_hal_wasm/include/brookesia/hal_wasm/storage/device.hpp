/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class StorageKeyValueWasmImpl;
class StorageFileSystemWasmImpl;

class StorageWasmDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "StorageWasm";
    static constexpr const char *KEY_VALUE_IFACE_NAME = "StorageWasm:KeyValue";
    static constexpr const char *FILE_SYSTEM_IFACE_NAME = "StorageWasm:FileSystem";

    StorageWasmDevice(const StorageWasmDevice &) = delete;
    StorageWasmDevice &operator=(const StorageWasmDevice &) = delete;
    StorageWasmDevice(StorageWasmDevice &&) = delete;
    StorageWasmDevice &operator=(StorageWasmDevice &&) = delete;

    static StorageWasmDevice &get_instance()
    {
        static StorageWasmDevice instance;
        return instance;
    }

private:
    StorageWasmDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~StorageWasmDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<StorageKeyValueWasmImpl> key_value_iface_;
    std::shared_ptr<StorageFileSystemWasmImpl> file_system_iface_;
};

} // namespace esp_brookesia::hal
