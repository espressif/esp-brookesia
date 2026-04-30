/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Singleton HAL device that aggregates codec-based audio playback and recording for board-managed audio hardware.
 */
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/display/backlight.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed custom display device
 *
 * Obtained via get_instance(). Not copyable or movable.
 */
class CustomDisplayDevice : public Device {
public:
    /** @brief Logical device name passed to the base @ref Device constructor. */
    static constexpr const char *DEVICE_NAME = "CustomDisplay";
    /** @brief Registry key for the display backlight HAL implementation (`"CustomDisplay:Backlight"`). */
    static constexpr const char *DISPLAY_BACKLIGHT_IMPL_NAME = "CustomDisplay:Backlight";

    CustomDisplayDevice(const CustomDisplayDevice &) = delete;
    CustomDisplayDevice &operator=(const CustomDisplayDevice &) = delete;
    CustomDisplayDevice(CustomDisplayDevice &&) = delete;
    CustomDisplayDevice &operator=(CustomDisplayDevice &&) = delete;

    /**
     * @brief Returns the process-wide singleton custom display device.
     *
     * @return Reference to the unique @ref CustomDisplayDevice instance.
     */
    static CustomDisplayDevice &get_instance()
    {
        static CustomDisplayDevice instance;
        return instance;
    }

private:
    CustomDisplayDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~CustomDisplayDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    bool init_backlight();
    void deinit_backlight();
};

} // namespace esp_brookesia::hal
