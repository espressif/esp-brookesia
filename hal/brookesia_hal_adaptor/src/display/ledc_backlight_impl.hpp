/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include "brookesia/hal_adaptor/macro_configs.h"
#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/hal_interface/interfaces/display/backlight.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed backlight HAL interface (holds LEDC handle, not the device object).
 */
class LedcDisplayBacklightImpl : public DisplayBacklightIface {
public:
    LedcDisplayBacklightImpl();
    ~LedcDisplayBacklightImpl() override;

    bool set_brightness(uint8_t percent) override;
    bool get_brightness(uint8_t &percent) override;

    bool is_light_on_off_supported() override
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        return is_backlight_control_valid_internal();
    }
    bool set_light_on_off(bool on) override;
    bool is_light_on() const override
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        return is_light_on_;
    }

    bool is_valid() const
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        return is_ledc_valid_internal();
    }

private:
    bool setup_ledc();
    bool setup_backlight_control();

    bool set_brightness_internal(uint8_t percent, bool force = false);
    bool light_on_off_internal(bool on, bool force = false);

    bool is_ledc_valid_internal() const
    {
        return (ledc_dev_handle_ != nullptr) && (ledc_periph_config_ != nullptr);
    }

    bool is_backlight_control_valid_internal() const
    {
        return backlight_control_handle_ != nullptr;
    }

    mutable boost::mutex mutex_;

    void *ledc_dev_handle_ = nullptr;
    void *ledc_periph_config_ = nullptr;
    uint8_t ledc_brightness_ = 0;

    void *backlight_control_handle_ = nullptr;
    bool backlight_control_active_level_ = true;
    bool is_light_on_ = false;
};

} // namespace esp_brookesia::hal
