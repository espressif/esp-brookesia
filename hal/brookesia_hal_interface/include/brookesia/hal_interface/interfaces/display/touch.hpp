/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file touch.hpp
 * @brief Declares the display touch-input HAL interface.
 */

namespace esp_brookesia::hal::display {

/**
 * @brief Touch-input interface paired with a display.
 */
class TouchIface: public Interface {
public:
    static constexpr const char *NAME = "DisplayTouch";  ///< Interface registry name.

    /**
     * @brief Supported event acquisition mode.
     */
    enum class OperationMode : uint8_t {
        Polling,       ///< Read touch points by polling.
        Interrupt,     ///< Read touch points by interrupt.
        Max,
    };

    /**
     * @brief Static touch capability information.
     */
    struct Info {
        uint16_t x_max = 0;              ///< Maximum X coordinate value.
        uint16_t y_max = 0;              ///< Maximum Y coordinate value.
        uint8_t max_points = 1;          ///< Maximum number of simultaneous touch points.
        OperationMode operation_mode = OperationMode::Max; ///< Supported acquisition mode.
        std::string group_id;            ///< Stable physical display group identifier.

        bool is_valid() const
        {
            return (x_max > 0) && (y_max > 0) && (max_points > 0) && (operation_mode != OperationMode::Max);
        }
    };

    /**
     * @brief A single touch point sample.
     */
    struct Point {
        int16_t x = 0;         ///< X coordinate.
        int16_t y = 0;         ///< Y coordinate.
        uint16_t pressure = 0; ///< Pressure or strength value from controller.
        uint8_t track_id = 0;  ///< Hardware track identifier for multi-touch controllers.
    };

    /**
     * @brief Driver-specific data.
     */
    struct DriverSpecific {
        void *io_handle = nullptr;    ///< Handle of the IO.
        void *touch_handle = nullptr; ///< Handle of the touch.
    };

    using InterruptHandler = bool (*)(void *ctx);

    /**
     * @brief Construct a touch-input interface.
     *
     * @param[in] info Static touch capability information.
     */
    TouchIface(Info info)
        : Interface(NAME)
        , info_(std::move(info))
    {
    }

    /**
     * @brief Virtual destructor for polymorphic touch interfaces.
     */
    virtual ~TouchIface() = default;

    /**
     * @brief Read current touch points.
     *
     * @param[out] points Output touch points.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool read_points(std::vector<Point> &points) = 0;

    /**
     * @brief Register an interrupt handler.
     *
     * The handler is called from ISR context and must only do ISR-safe work.
     *
     * @param[in] handler Interrupt handler. If `nullptr`, the interrupt handler will be unregistered.
     * @param[in] ctx User context passed to @p handler.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool register_interrupt_handler(InterruptHandler handler, void *ctx) = 0;

    /**
     * @brief Get the driver-specific data.
     *
     * @param[out] specific The driver-specific data.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool get_driver_specific(DriverSpecific &specific)
    {
        (void)specific;
        return false;
    }

    /**
     * @brief Get static touch capability information.
     *
     * @return Touch information.
     */
    const Info &get_info() const
    {
        return info_;
    }

protected:
    InterruptHandler interrupt_handler_ = nullptr;
    void *interrupt_handler_ctx_ = nullptr;

private:
    Info info_{};
};

BROOKESIA_DESCRIBE_ENUM(TouchIface::OperationMode, Polling, Interrupt, Max);
BROOKESIA_DESCRIBE_STRUCT(TouchIface::Info, (), (x_max, y_max, max_points, operation_mode, group_id));
BROOKESIA_DESCRIBE_STRUCT(TouchIface::Point, (), (x, y, pressure, track_id));

} // namespace esp_brookesia::hal::display
