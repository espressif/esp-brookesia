/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lcd_display_device.hpp
 * @brief Board-backed LCD adaptor that exposes panel and touch HAL interfaces.
 */
#pragma once

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interface.hpp"

using namespace esp_brookesia::hal;

constexpr const char *DISPLAY_DEVICE = "LcdDisplay"; ///< Registry name used when auto-registering the LCD adaptor.

/**
 * @brief LCD adaptor backed by board-managed display, touch, and brightness devices.
 *
 * The adaptor exposes both `DisplayPanelIface` and `DisplayTouchIface` and
 * bridges them to the corresponding board-manager devices.
 */
class LCDDisplayDevice : public DeviceImpl<LCDDisplayDevice>, public DisplayPanelIface, public DisplayTouchIface {
public:
    /**
     * @brief Constructs the LCD adaptor.
     */
    LCDDisplayDevice(): DeviceImpl<LCDDisplayDevice>(DISPLAY_DEVICE) {}

    /**
     * @brief Destroys the LCD adaptor.
     */
    ~LCDDisplayDevice() = default;

    /**
     * @brief Reports whether the adaptor is supported on the current board.
     *
     * @return Always true for this adaptor implementation.
     */
    bool probe() override
    {
        return true;
    }

    /**
     * @brief Returns whether the display and touch resources have been initialized.
     *
     * @return true when required handles and configuration are available, otherwise false.
     */
    bool check_initialized() const override;

    /**
     * @brief Initializes the display, touch, and backlight resources.
     *
     * @return true on success, otherwise false.
     */
    bool init() override;

    /**
     * @brief Deinitializes the display, touch, and backlight resources.
     *
     * @return true on success, otherwise false.
     */
    bool deinit() override;

    /**
     * @brief Sets the display backlight brightness.
     *
     * @param percent Requested brightness in percent.
     * @return true on success, otherwise false.
     */
    bool set_backlight(int percent) override;

    /**
     * @brief Draws a bitmap into the target panel region.
     *
     * @param x1 Left coordinate of the target region.
     * @param y1 Top coordinate of the target region.
     * @param x2 Right coordinate of the target region.
     * @param y2 Bottom coordinate of the target region.
     * @param data Pointer to the source pixel buffer.
     * @return true on success, otherwise false.
     */
    bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const void *data) override;

    /**
     * @brief Registers display-transfer completion callbacks.
     *
     * @param callbacks Callback collection to install.
     * @return true on success, otherwise false.
     */
    bool register_callbacks(const DisplayCallbacks &callbacks) override;

private:
    /**
     * @brief Resolves runtime interface queries supported by this adaptor.
     *
     * @param id Hashed interface identifier.
     * @return Pointer to the matching interface, or `nullptr` when unsupported.
     */
    void *query_interface(uint64_t id) override
    {
        return build_table<DisplayPanelIface, DisplayTouchIface>(id);
    }

    /**
     * @brief Internal unlocked variant of `check_initialized()`.
     *
     * @return true when display resources are ready for use.
     */
    bool check_initialized_intern() const;

    mutable boost::mutex mutex_;                 ///< Mutex guarding adaptor state.

    void *ledc_handle = nullptr; ///< Cached opaque LEDC/backlight handle.
};
