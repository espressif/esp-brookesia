/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/hal_interface/display/backlight.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed backlight HAL interface (holds LEDC handle, not the device object).
 */
class CustomDisplayBacklightImpl : public DisplayBacklightIface {
public:
    CustomDisplayBacklightImpl(std::optional<DisplayBacklightIface::Info> info);
    ~CustomDisplayBacklightImpl() override;

    bool set_brightness(uint8_t percent) override;
    bool get_brightness(uint8_t &percent) override;
    bool turn_on() override;
    bool turn_off() override;

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

    bool set_brightness_internal(uint8_t percent, bool need_map = true);

    bool get_brightness_internal(uint8_t &percent) const;

    mutable boost::mutex mutex_;
    void *handles_ = nullptr;
    uint8_t brightness_percent_ = std::numeric_limits<uint8_t>::max();        // Initialized to max to indicate not set
    uint8_t brightness_before_off_ = 0;     // external brightness saved before turn_off()
};

} // namespace esp_brookesia::hal
