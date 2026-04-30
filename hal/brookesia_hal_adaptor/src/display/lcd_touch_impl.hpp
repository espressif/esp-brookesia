/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed touch HAL interface (holds touch handle, not the device object).
 */
class I2cDisplayTouchImpl : public DisplayTouchIface {
public:
    I2cDisplayTouchImpl();
    ~I2cDisplayTouchImpl() override;

    bool read_points(std::vector<DisplayTouchIface::Point> &points) override;

    bool get_driver_specific(DriverSpecific &specific) override;

    bool register_interrupt_handler(InterruptHandler handler) override;

    bool is_valid() const
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        return is_valid_internal();
    }

private:
    bool is_valid_internal() const
    {
        return handles_ != nullptr;
    }

    mutable boost::mutex mutex_;
    void *handles_ = nullptr;
};

} // namespace esp_brookesia::hal
