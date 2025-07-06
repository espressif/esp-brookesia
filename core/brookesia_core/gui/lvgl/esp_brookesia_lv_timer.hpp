/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "lvgl.h"
#include "style/esp_brookesia_gui_style.hpp"

namespace esp_brookesia::gui {

class LvTimer {
public:
    struct TimerUserData {
        LvTimer *timer = nullptr;
        void *user_data = nullptr;
    };

    using TimerCallback = std::function<void(void *)>;

    LvTimer(TimerCallback callback, uint32_t period, void *user_data);
    ~LvTimer();

    /**
     * @brief Disable copy operations
     */
    LvTimer(const LvTimer &other) = delete;
    LvTimer &operator=(const LvTimer &other) = delete;

    /**
     * @brief Enable move operations
     */
    LvTimer(LvTimer &&other):
        _native_handle(other._native_handle)
    {
        other._native_handle = nullptr;
    }
    LvTimer &operator=(LvTimer &&other)
    {
        if (this != &other) {
            _native_handle = other._native_handle;
            other._native_handle = nullptr;
        }
        return *this;
    }

    bool pause();
    bool resume();
    bool restart();
    bool reset();
    bool setInterval(uint32_t interval_ms);

    bool isValid() const
    {
        return (_native_handle != nullptr);
    }

private:
    lv_timer_t *_native_handle = nullptr;
    TimerCallback _callback = nullptr;
    TimerUserData _user_data{};
};

using LvTimerUniquePtr = std::unique_ptr<LvTimer>;

} // namespace esp_brookesia::gui
