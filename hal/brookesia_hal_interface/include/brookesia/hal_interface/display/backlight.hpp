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
     * @brief Static backlight capability information.
     */
    struct Info {
        uint8_t brightness_default; ///< Default brightness percentage.
        uint8_t brightness_min;     ///< Minimum brightness percentage.
        uint8_t brightness_max;     ///< Maximum brightness percentage.
    };

    /**
     * @brief Construct a display backlight interface.
     *
     * @param[in] info Static backlight capability information.
     */
    DisplayBacklightIface(Info info)
        : Interface(NAME)
        , info_(std::move(info))
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
     * @brief Turn on the backlight.
     *
     * @note Calling this interface will restore the brightness to the last set value. If the last set value is zero,
     *       the brightness will be restored to the default value.
     *
     * @return `true` on success; otherwise `false`.
     */
    virtual bool turn_on() = 0;

    /**
     * @brief Turn off the backlight.
     *
     * @note Calling this interface will turn off the backlight. Even if the minimum brightness is not zero,
     *       the brightness will be turned off to zero.
     *
     * @return `true` on success; otherwise `false`.
     */
    virtual bool turn_off() = 0;

    /**
     * @brief Get static backlight capability information.
     *
     * @return Backlight information.
     */
    const Info &get_info() const
    {
        return info_;
    }

private:
    Info info_{};
};

BROOKESIA_DESCRIBE_STRUCT(DisplayBacklightIface::Info, (), (brightness_default, brightness_min, brightness_max));

} // namespace esp_brookesia::hal
