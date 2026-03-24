/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file display_panel.hpp
 * @brief Display panel interface definitions for HAL devices.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Abstract display-panel interface exposed by display-capable devices.
 */
class DisplayPanelIface {
public:
    static constexpr std::string_view interface_name = "DisplayPanelIface"; ///< Unique interface name used for runtime lookup.

    /**
     * @brief Static display characteristics reported by the implementation.
     */
    struct DisplayPeripheralConfig {
        uint32_t h_res;                 ///< Horizontal resolution in pixels.
        uint32_t v_res;                 ///< Vertical resolution in pixels.
        bool flag_swap_color_bytes;     ///< Whether the display transport expects swapped color-byte order.
    };

    using DisplayBitmapFlushDoneCallback = std::function<bool()>; ///< Callback invoked when an asynchronous bitmap flush completes.

    /**
     * @brief Callback collection used by the display implementation.
     */
    struct DisplayCallbacks {
        DisplayBitmapFlushDoneCallback bitmap_flush_done; ///< Optional callback fired after a bitmap transfer completes.
    };

    /**
     * @brief Constructs an empty display-panel interface.
     */
    DisplayPanelIface() = default;

    /**
     * @brief Destroys the display-panel interface.
     */
    virtual ~DisplayPanelIface() = default;

    /**
     * @brief Returns the current display configuration.
     *
     * @return Immutable reference to the display configuration.
     */
    const DisplayPeripheralConfig &get_config() const
    {
        return config_;
    }

    /**
     * @brief Sets the panel backlight brightness.
     *
     * @param percent Brightness in percent, typically within the range `[0, 100]`.
     * @return true on success, otherwise false.
     */
    virtual bool set_backlight(int percent) = 0;

    /**
     * @brief Draws a bitmap into the rectangular region `[x1, y1) - [x2, y2)`.
     *
     * @param x1 Left coordinate of the target region.
     * @param y1 Top coordinate of the target region.
     * @param x2 Right coordinate of the target region.
     * @param y2 Bottom coordinate of the target region.
     * @param data Pointer to the source pixel buffer.
     * @return true on success, otherwise false.
     */
    virtual bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const void *data) = 0;

    /**
     * @brief Registers panel event callbacks.
     *
     * @param callbacks Callback collection to install.
     * @return true on success, otherwise false.
     */
    virtual bool register_callbacks(const DisplayCallbacks &callbacks) = 0;

protected:
    DisplayPeripheralConfig config_ = {}; ///< Cached display configuration.
    DisplayCallbacks display_callbacks_{}; ///< Callback set installed by upper layers.
    void *display_lcd_handles_ = nullptr; ///< Opaque implementation-defined LCD handle bundle.
    void *display_lcd_cfg_ = nullptr;     ///< Opaque implementation-defined LCD configuration handle.
};

BROOKESIA_DESCRIBE_STRUCT(DisplayPanelIface::DisplayPeripheralConfig, (), (h_res, v_res, flag_swap_color_bytes));

BROOKESIA_DESCRIBE_STRUCT(DisplayPanelIface::DisplayCallbacks, (), (bitmap_flush_done));

} // namespace esp_brookesia::hal
