/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_helper/media/display.hpp"
#include "brookesia/service_manager.hpp"

class Display {
public:
    using GestureData = esp_brookesia::service::helper::Display::TouchGestureConfig;
    using GestureInfo = esp_brookesia::service::helper::Display::TouchGestureInfo;

    static Display &get_instance()
    {
        static Display instance;
        return instance;
    }

    struct Config {
        std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler;
        int lvgl_task_core = 0;
        GestureData gesture_data{};
    };

    bool start(const Config &config);

    uint32_t width() const
    {
        return display_width_;
    }

    uint32_t height() const
    {
        return display_height_;
    }

    const std::string &output_name() const
    {
        return display_output_name_;
    }

private:
    Display() = default;
    ~Display() = default;
    Display(const Display &) = delete;
    Display(Display &&) = delete;
    Display &operator=(const Display &) = delete;
    Display &operator=(Display &&) = delete;

    bool start_display_service();
    bool start_lvgl_display_source(int core_id);
    bool set_active_lvgl_source();
    bool start_gesture();
    void handle_touch_gesture_event(const std::string &event_name, const GestureInfo &info);

    std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler_;
    esp_brookesia::service::ServiceBinding display_service_binding_;
    esp_brookesia::service::EventRegistry::SignalConnection gesture_event_connection_;
    std::string display_output_name_;
    uint32_t display_output_id_ = 0;
    uint32_t display_width_ = 0;
    uint32_t display_height_ = 0;
    bool display_backlight_on_off_supported_ = false;
    GestureData gesture_data_{};
};
