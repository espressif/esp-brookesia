/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file display_touch.hpp
 * @brief Display-touch interface definitions for HAL devices.
 */
#include <string_view>

namespace esp_brookesia::hal {

/**
 * @brief Abstract touch-input interface paired with a display device.
 */
class DisplayTouchIface {
public:
    static constexpr std::string_view interface_name = "DisplayTouchIface"; ///< Unique interface name used for runtime lookup.

    /**
     * @brief Constructs an empty touch interface.
     */
    DisplayTouchIface() = default;

    /**
     * @brief Destroys the touch interface.
     */
    virtual ~DisplayTouchIface() = default;

    /**
     * @brief Returns the implementation-defined native touch handle.
     *
     * @return Opaque handle managed by the device implementation, or `nullptr` when unavailable.
     */
    const void *get_handle() const
    {
        return display_touch_handles_;
    }

protected:
    void *display_touch_handles_ = nullptr; ///< Opaque implementation-defined touch handle.
};

} // namespace esp_brookesia::hal
