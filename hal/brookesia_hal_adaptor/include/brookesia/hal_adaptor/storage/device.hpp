/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Singleton HAL device that registers board-backed storage interfaces.
 */
#pragma once

#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed storage device: publishes filesystem and key-value HAL interfaces after bring-up.
 *
 * Obtained via get_instance(). Not copyable or movable.
 */
class StorageDevice : public Device {
public:
    /** @brief Logical device name passed to the base @ref Device constructor. */
    static constexpr const char *DEVICE_NAME = "Storage";
    static constexpr const char *FILE_SYSTEM_IFACE_NAME = "Storage:FileSystem";
    static constexpr const char *KEY_VALUE_IFACE_NAME = "Storage:KeyValue";

    StorageDevice(const StorageDevice &) = delete;
    StorageDevice &operator=(const StorageDevice &) = delete;
    StorageDevice(StorageDevice &&) = delete;
    StorageDevice &operator=(StorageDevice &&) = delete;

    /**
     * @brief Returns the process-wide singleton storage device.
     *
     * @return Reference to the unique @ref StorageDevice instance.
     */
    static StorageDevice &get_instance()
    {
        static StorageDevice instance;
        return instance;
    }

private:
    StorageDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~StorageDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    bool init_file_system();
    void deinit_file_system();
    bool init_key_value();
    void deinit_key_value();
};

} // namespace esp_brookesia::hal
