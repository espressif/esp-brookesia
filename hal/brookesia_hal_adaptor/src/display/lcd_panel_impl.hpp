/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed panel HAL interface (holds LCD board handles, not the device object).
 */
class LcdDisplayPanelImpl : public DisplayPanelIface {
public:
    LcdDisplayPanelImpl();
    ~LcdDisplayPanelImpl() override;

    bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data) override;

    bool get_driver_specific(DriverSpecific &specific) override;

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
