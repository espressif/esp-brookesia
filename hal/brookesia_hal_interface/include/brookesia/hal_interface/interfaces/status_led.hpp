/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file status_led.hpp
 * @brief Status LED interface definitions for HAL devices.
 */
#pragma once

#include <string_view>
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Abstract status LED interface exposed by devices with indicator LEDs.
 */
class StatusLedIface {
public:
    static constexpr std::string_view interface_name = "StatusLedIface"; ///< Unique interface name used for runtime lookup.

    /**
     * @brief Constructs an empty status LED interface.
     */
    StatusLedIface() = default;

    /**
     * @brief Destroys the status LED interface.
     */
    virtual ~StatusLedIface() = default;

    /**
     * @brief Predefined LED blink patterns supported by the interface.
     */
    enum class BlinkType {
        LowPower = 0,    ///< Low-power indication.
        DevelopMode,     ///< Development or debug mode indication.
        TouchPressDown,  ///< Touch-press feedback indication.
        WifiConnected,   ///< Wi-Fi connected indication.
        WifiDisconnected, ///< Wi-Fi disconnected indication.
        Max,             ///< Sentinel value marking the end of the enum.
    };

    /**
     * @brief Starts the blink pattern associated with type.
     *
     * @param type Blink pattern to enable.
     */
    virtual void start_blink(BlinkType type) = 0;

    /**
     * @brief Stops the blink pattern associated with type.
     *
     * @param type Blink pattern to disable.
     */
    virtual void stop_blink(BlinkType type) = 0;
};

BROOKESIA_DESCRIBE_ENUM(
    StatusLedIface::BlinkType,
    LowPower, DevelopMode, TouchPressDown, WifiConnected, WifiDisconnected, Max
);

} // namespace esp_brookesia::hal
