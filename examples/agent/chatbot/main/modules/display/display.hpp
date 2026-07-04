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
#include "brookesia/lib_utils/state_machine.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper/media/display.hpp"
#include "screens/common.hpp"

class Display {
public:
    friend class ScreenEmote;
    friend class ScreenSettings;

    static constexpr const char *TASK_GROUP_NAME = "Display";

    using GestureDirection = esp_brookesia::service::helper::Display::TouchGestureDirection;
    using GestureArea = esp_brookesia::service::helper::Display::TouchGestureArea;
    using GestureEventType = esp_brookesia::service::helper::Display::TouchGestureEventType;
    using GestureData = esp_brookesia::service::helper::Display::TouchGestureConfig;
    using GestureInfo = esp_brookesia::service::helper::Display::TouchGestureInfo;

    static Display &get_instance()
    {
        static Display instance;
        return instance;
    }

    struct Config {
        std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler;
        GestureData gesture_data{};
    };

    bool start(const Config &config);
    bool show_video();
    bool show_emote();
    const std::string &output_name() const
    {
        return display_output_name_;
    }
    uint32_t width() const
    {
        return display_width_;
    }
    uint32_t height() const
    {
        return display_height_;
    }

private:
    enum class DrawSource : uint8_t {
        Lvgl,
        Emote,
        Video,
    };

    Display() = default;
    ~Display() = default;

    Display(const Display &) = delete;
    Display(Display &&) = delete;
    Display &operator=(const Display &) = delete;
    Display &operator=(Display &&) = delete;

    bool start_lvgl_display_source();
    bool start_display_service();
    bool start_expression_emote_assets();
    bool start_gesture();
    void stop_gesture();

    bool set_active_source_role(DrawSource source);

    bool start_ui_state_machine();
    DisplayAction get_ui_action_from_gesture(const GestureInfo &info) const;

    bool send_display_task(esp_brookesia::lib_utils::TaskScheduler::OnceTask &&task);

    std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler_;
    esp_brookesia::service::ServiceBinding display_service_binding_;
    esp_brookesia::service::ServiceBinding emote_service_binding_;

    std::string display_output_name_;
    uint32_t display_output_id_ = 0;
    uint32_t display_width_ = 0;
    uint32_t display_height_ = 0;

    GestureData gesture_data_{};
    esp_brookesia::service::EventRegistry::SignalConnection gesture_event_connection_;

    std::unique_ptr<esp_brookesia::lib_utils::StateMachine> ui_state_machine_;
    bool is_ui_state_action_triggered_ = false;
};
