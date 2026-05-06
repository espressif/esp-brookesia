/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <string_view>
#include <utility>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file backlight.hpp
 * @brief Declares the display backlight HAL interface.
 */

namespace esp_brookesia::hal {

/**
 * @brief Display backlight control interface.
 */
class DisplayBacklightIface: public Interface {
public:
    static constexpr const char *NAME = "DisplayBacklight";  ///< Interface registry name.

    /**
     * @brief Construct a display backlight interface.
     *
     */
    DisplayBacklightIface()
        : Interface(NAME)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic backlight interfaces.
     */
    virtual ~DisplayBacklightIface() = default;

    /**
     * @brief Set backlight brightness.
     *
     * @param[in] percent Brightness percentage.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool set_brightness(uint8_t percent) = 0;

    /**
     * @brief Get current backlight brightness.
     *
     * @param[out] percent Current brightness percentage.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool get_brightness(uint8_t &percent) = 0;

    /**
     * @brief Check if the light on/off control is supported.
     *
     * @return `true` if the light on/off control is supported; otherwise `false`.
     */
    virtual bool is_light_on_off_supported() = 0;

    /**
     * @brief Set the backlight on/off state.
     *
     * @param[in] on `true` to set the backlight on; `false` to set the backlight off.
     * @return `true` if the backlight on/off state is set successfully; otherwise `false`.
     */
    virtual bool set_light_on_off(bool on) = 0;

    /**
     * @brief Check if the backlight is set on.
     *
     * @return `true` if the backlight is set on; otherwise `false`.
     */
    virtual bool is_light_on() const = 0;
};

} // namespace esp_brookesia::hal
