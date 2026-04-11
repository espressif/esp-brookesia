/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Singleton HAL device that registers a general-purpose filesystem interface for board-backed storage.
 */
#pragma once

#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed storage device: publishes a general filesystem HAL interface after bring-up.
 *
 * Obtained via get_instance(). Not copyable or movable.
 */
class StorageDevice : public Device {
public:
    /** @brief Logical device name passed to the base @ref Device constructor. */
    static constexpr const char *DEVICE_NAME = "Storage";
    /** @brief Registry key for the general filesystem HAL interface (`"Storage:GenralFS"`). */
    static constexpr const char *GENERAL_FS_IMPL_NAME = "Storage:GenralFS";

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
    bool on_init() override;
    void on_deinit() override;

    bool init_general_fs();
    void deinit_general_fs();
};

} // namespace esp_brookesia::hal
