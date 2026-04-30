/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/hal_interface/interfaces/display/backlight.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed backlight HAL interface (holds LEDC handle, not the device object).
 */
class CustomDisplayBacklightImpl : public DisplayBacklightIface {
public:
    CustomDisplayBacklightImpl();
    ~CustomDisplayBacklightImpl() override;

    bool set_brightness(uint8_t percent) override;
    bool get_brightness(uint8_t &percent) override;

    bool is_light_on_off_supported() override
    {
        return false;
    }
    bool set_light_on_off(bool on) override
    {
        return false;
    }
    bool is_light_on() const override
    {
        return false;
    }

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

    bool set_brightness_internal(uint8_t percent, bool force = false);

    mutable boost::mutex mutex_;
    void *handles_ = nullptr;
    uint8_t brightness_ = 0;
};

} // namespace esp_brookesia::hal
