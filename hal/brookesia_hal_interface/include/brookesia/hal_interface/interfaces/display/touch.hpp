/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file touch.hpp
 * @brief Declares the display touch-input HAL interface.
 */

namespace esp_brookesia::hal {

/**
 * @brief Touch-input interface paired with a display.
 */
class DisplayTouchIface: public Interface {
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
        OperationMode operation_mode = OperationMode::Max; ///< Supported acquisition mode.

        bool is_valid() const
        {
            return (x_max > 0) && (y_max > 0) && (operation_mode != OperationMode::Max);
        }
    };

    /**
     * @brief A single touch point sample.
     */
    struct Point {
        int16_t x;         ///< X coordinate.
        int16_t y;         ///< Y coordinate.
        uint16_t pressure; ///< Pressure or strength value from controller.
    };

    /**
     * @brief Driver-specific data.
     */
    struct DriverSpecific {
        void *io_handle = nullptr;    ///< Handle of the IO.
        void *touch_handle = nullptr; ///< Handle of the touch.
    };

    using InterruptHandler = std::function<void()>;

    /**
     * @brief Construct a touch-input interface.
     *
     * @param[in] info Static touch capability information.
     */
    DisplayTouchIface(Info info)
        : Interface(NAME)
        , info_(std::move(info))
    {
    }

    /**
     * @brief Virtual destructor for polymorphic touch interfaces.
     */
    virtual ~DisplayTouchIface() = default;

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
     * @param[in] handler Interrupt handler. If `nullptr`, the interrupt handler will be unregistered.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool register_interrupt_handler(InterruptHandler handler) = 0;

    /**
     * @brief Get the driver-specific data.
     *
     * @param[out] specific The driver-specific data.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool get_driver_specific(DriverSpecific &specific)
    {
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
    InterruptHandler interrupt_handler_;

private:
    Info info_{};
};

BROOKESIA_DESCRIBE_ENUM(DisplayTouchIface::OperationMode, Polling, Interrupt, Max);
BROOKESIA_DESCRIBE_STRUCT(DisplayTouchIface::Info, (), (x_max, y_max, operation_mode));

} // namespace esp_brookesia::hal
