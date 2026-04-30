/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Singleton HAL device that aggregates LCD panel, touch, and LEDC backlight implementations for board-managed display hardware.
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
 * @brief Board-backed display device: registers panel, touch, and backlight HAL interfaces after board bring-up.
 *
 * Obtained via get_instance(). Not copyable or movable.
 */
class DisplayDevice : public Device {
public:
    /** @brief Logical device name passed to the base @ref Device constructor. */
    static constexpr const char *DEVICE_NAME = "Display";
    /** @brief Registry key for the LEDC-based backlight HAL implementation (`"Display:LedcBacklight"`). */
    static constexpr const char *LEDC_BACKLIGHT_IMPL_NAME = "Display:LedcBacklight";
    /** @brief Registry key for the LCD panel HAL implementation (`"Display:LcdPanel"`). */
    static constexpr const char *LCD_PANEL_IMPL_NAME = "Display:LcdPanel";
    /** @brief Registry key for the LCD touch HAL implementation (`"Display:LcdTouch"`). */
    static constexpr const char *LCD_TOUCH_IMPL_NAME = "Display:LcdTouch";

    DisplayDevice(const DisplayDevice &) = delete;
    DisplayDevice &operator=(const DisplayDevice &) = delete;
    DisplayDevice(DisplayDevice &&) = delete;
    DisplayDevice &operator=(DisplayDevice &&) = delete;

    /**
     * @brief Returns the process-wide singleton display device.
     *
     * @return Reference to the unique @ref DisplayDevice instance.
     */
    static DisplayDevice &get_instance()
    {
        static DisplayDevice instance;
        return instance;
    }

private:
    DisplayDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~DisplayDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    bool init_ledc_backlight();
    void deinit_ledc_backlight();

    bool init_lcd_panel();
    void deinit_lcd_panel();

    bool init_lcd_touch();
    void deinit_lcd_touch();
};

} // namespace esp_brookesia::hal
