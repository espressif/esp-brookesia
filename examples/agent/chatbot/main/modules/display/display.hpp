/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include "boost/signals2/connection.hpp"
#include "boost/signals2/signal.hpp"
#include "boost/thread/thread.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/state_machine.hpp"
#include "brookesia/hal_interface.hpp"
#include "screens/common.hpp"

class Display {
public:
    friend class ScreenEmote;
    friend class ScreenSettings;

    static constexpr const char *TASK_GROUP_NAME = "Display";

    enum class GestureDirection : uint8_t {
        None  = 0,
        Up    = (1 << 0),
        Down  = (1 << 1),
        Left  = (1 << 2),
        Right = (1 << 3),
        Hor   = (static_cast<uint8_t>(Left) | static_cast<uint8_t>(Right)),
        Ver   = (static_cast<uint8_t>(Up) | static_cast<uint8_t>(Down)),
    };

    enum class GestureArea : uint8_t {
        Center     = 0,
        TopEdge    = (1 << 0),
        BottomEdge = (1 << 1),
        LeftEdge   = (1 << 2),
        RightEdge  = (1 << 3),
    };

    struct GestureDataThreshold {
        uint8_t direction_angle = 45;
        uint16_t direction_vertical = 0;
        uint16_t direction_horizon = 0;
        uint16_t horizontal_edge = 0;
        uint16_t vertical_edge = 0;
        uint16_t duration_short_ms = 220;
        float speed_slow_px_per_ms = 0.6F;
    };
    struct GestureData {
        uint16_t detect_period_ms = 20;
        GestureDataThreshold threshold;
    };

    struct GestureInfo {
        GestureDirection direction = GestureDirection::None;
        uint8_t start_area = static_cast<uint8_t>(GestureArea::Center);
        uint8_t stop_area = static_cast<uint8_t>(GestureArea::Center);
        int start_x = -1;
        int start_y = -1;
        int stop_x = -1;
        int stop_y = -1;
        uint32_t duration_ms = 0;
        float speed_px_per_ms = 0;
        float distance_px = 0;
        bool flags_slow_speed = false;
        bool flags_short_duration = false;
    };

    using GestureSignal = boost::signals2::signal<void(const GestureInfo &)>;
    using GestureSignalConnection = boost::signals2::connection;

    static Display &get_instance()
    {
        static Display instance;
        return instance;
    }

    struct Config {
        std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler;
        int lvgl_task_core = 0;
        int emote_task_core = 0;
        esp_brookesia::lib_utils::ThreadConfig gesture_thread_config{
            .name = "DisplayGesture",
            .core_id = -1,
            .priority = 5,
            .stack_size = 10 * 1024,
        };
        GestureData gesture_data{};
    };

    bool start(const Config &config);

    GestureSignalConnection connect_gesture_press_signal(const GestureSignal::slot_type &slot)
    {
        return gesture_press_signal_.connect(slot);
    }
    GestureSignalConnection connect_gesture_pressing_signal(const GestureSignal::slot_type &slot)
    {
        return gesture_pressing_signal_.connect(slot);
    }
    GestureSignalConnection connect_gesture_release_signal(const GestureSignal::slot_type &slot)
    {
        return gesture_release_signal_.connect(slot);
    }

private:
    Display() = default;
    ~Display() = default;

    Display(const Display &) = delete;
    Display(Display &&) = delete;
    Display &operator=(const Display &) = delete;
    Display &operator=(Display &&) = delete;

    void try_load_data_from_nvs();
    void try_reset_data();

    bool is_backlight_available() const
    {
        return (backlight_iface_ != nullptr);
    }
    bool set_brightness(uint8_t brightness);
    bool get_brightness(uint8_t &brightness);

    bool start_lvgl(int core_id);
    bool start_expression_emote(int core_id);
    bool start_gesture(const esp_brookesia::lib_utils::ThreadConfig &thread_config);
    void stop_gesture();

    bool start_ui_state_machine();
    DisplayAction get_ui_action_from_gesture(const GestureInfo &info) const;

    bool process_gesture_tick();
    uint8_t get_gesture_area(int x, int y) const;
    bool check_gesture_data(const GestureData &data) const;
    static GestureData build_default_gesture_data(uint32_t h_res, uint32_t v_res);

    bool send_display_task(esp_brookesia::lib_utils::TaskScheduler::OnceTask &&task);

    std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler_;
    std::shared_ptr<esp_brookesia::hal::DisplayPanelIface> display_iface_;
    std::shared_ptr<esp_brookesia::hal::DisplayTouchIface> touch_iface_;
    std::shared_ptr<esp_brookesia::hal::DisplayBacklightIface> backlight_iface_;

    boost::mutex gesture_mutex_;
    GestureData gesture_data_{};
    GestureInfo gesture_info_{};
    float gesture_direction_tan_threshold_ = 1.0F;
    uint64_t gesture_touch_start_time_ms_ = 0;
    bool gesture_detection_started_ = false;
    boost::thread gesture_thread_{};
    GestureSignal gesture_press_signal_{};
    GestureSignal gesture_pressing_signal_{};
    GestureSignal gesture_release_signal_{};

    std::unique_ptr<esp_brookesia::lib_utils::StateMachine> ui_state_machine_;
    GestureSignalConnection ui_state_gesture_pressing_connection_;
    GestureSignalConnection ui_state_gesture_release_connection_;
    bool is_ui_state_action_triggered_ = false;
};

BROOKESIA_DESCRIBE_ENUM(Display::GestureDirection, None, Up, Down, Left, Right, Hor, Ver)
BROOKESIA_DESCRIBE_ENUM(Display::GestureArea, Center, TopEdge, BottomEdge, LeftEdge, RightEdge)
BROOKESIA_DESCRIBE_STRUCT(
    Display::GestureInfo, (),
    (
        direction, start_area, stop_area, start_x, start_y, stop_x, stop_y, duration_ms, speed_px_per_ms, distance_px,
        flags_slow_speed, flags_short_duration
    )
)
BROOKESIA_DESCRIBE_STRUCT(Display::GestureData, (), (detect_period_ms, threshold))
BROOKESIA_DESCRIBE_STRUCT(
    Display::GestureDataThreshold, (),
    (
        direction_vertical, direction_horizon, direction_angle, horizontal_edge, vertical_edge, duration_short_ms,
        speed_slow_px_per_ms
    )
)
