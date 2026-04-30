/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Singleton HAL device that publishes power metadata from the power device.
 */
#pragma once

#include <string>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Power-backed metadata device: publishes a power HAL interface.
 *
 * Obtained via get_instance(). Not copyable or movable.
 */
class PowerDevice : public Device {
public:
    /** @brief Logical device name passed to the base @ref Device constructor. */
    static constexpr const char *DEVICE_NAME = "Power";
    /** @brief Registry key for the power HAL interface (`"Power:Battery"`). */
    static constexpr const char *BATTERY_IMPL_NAME = "Power:Battery";

    PowerDevice(const PowerDevice &) = delete;
    PowerDevice &operator=(const PowerDevice &) = delete;
    PowerDevice(PowerDevice &&) = delete;
    PowerDevice &operator=(PowerDevice &&) = delete;

    /**
     * @brief Returns the process-wide singleton power device.
     *
     * @return Reference to the unique @ref PowerDevice instance.
     */
    static PowerDevice &get_instance()
    {
        static PowerDevice instance;
        return instance;
    }

private:
    PowerDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~PowerDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    bool init_battery();
    void deinit_battery();
};

} // namespace esp_brookesia::hal
