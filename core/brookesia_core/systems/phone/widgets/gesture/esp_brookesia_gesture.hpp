/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/base/esp_brookesia_base_context.hpp"
#include "lvgl/esp_brookesia_lv_helper.hpp"

namespace esp_brookesia::systems::phone {

class Gesture {
public:
    enum class IndicatorBarType {
        LEFT = 0,
        RIGHT,
        BOTTOM,
        MAX,
    };

    struct IndicatorBarData {
        struct {
            gui::StyleSize size_min;
            gui::StyleSize size_max;
            uint8_t radius;
            uint8_t layout_pad_all;
            gui::StyleColor color;
        } main;
        struct {
            uint8_t radius;
            gui::StyleColor color;
        } indicator;
        struct {
            gui::StyleAnimation::AnimationPathType scale_back_path_type;
            uint32_t scale_back_time_ms;
        } animation;
    };

    struct Data {
        uint8_t detect_period_ms;
        struct {
            int direction_vertical;
            int direction_horizon;
            uint8_t direction_angle;
            int horizontal_edge;
            int vertical_edge;
            int duration_short_ms;
            float speed_slow_px_per_ms;
        } threshold;
        Gesture::IndicatorBarData indicator_bars[static_cast<int>(Gesture::IndicatorBarType::MAX)];
        struct {
            uint8_t enable_indicator_bars[static_cast<int>(Gesture::IndicatorBarType::MAX)];
        } flags;
    };

    enum Direction {
        DIR_NONE  = 0,
        DIR_UP    = (1 << 0),
        DIR_DOWN  = (1 << 1),
        DIR_LEFT  = (1 << 2),
        DIR_RIGHT = (1 << 3),
        DIR_HOR   = (DIR_LEFT | DIR_RIGHT),
        DIR_VER   = (DIR_UP | DIR_DOWN),
    };

    enum Area {
        AREA_CENTER      = 0,
        AREA_TOP_EDGE    = (1 << 0),
        AREA_BOTTOM_EDGE = (1 << 1),
        AREA_LEFT_EDGE   = (1 << 2),
        AREA_RIGHT_EDGE  = (1 << 3),
    };

    struct Info {
        Direction direction;
        uint8_t start_area;
        uint8_t stop_area;
        int start_x;
        int start_y;
        int stop_x;
        int stop_y;
        uint32_t duration_ms;
        float speed_px_per_ms;
        float distance_px;
        struct {
            uint8_t slow_speed: 1;
            uint8_t short_duration: 1;
        } flags;
    };

    Gesture(base::Context &core_in, const Gesture::Data &data_in);
    ~Gesture();

    bool readTouchPoint(int &x, int &y) const;

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool setMaskObjectVisible(bool visible) const;
    bool setIndicatorBarLength(Gesture::IndicatorBarType type, int length) const;
    bool setIndicatorBarLengthByOffset(Gesture::IndicatorBarType type, int offset) const;
    bool setIndicatorBarVisible(Gesture::IndicatorBarType type, bool visible);
    bool controlIndicatorBarScaleBackAnim(Gesture::IndicatorBarType type, bool start);

    bool checkInitialized(void) const
    {
        return (_event_mask_obj != nullptr);
    }
    bool checkGestureStart(void) const
    {
        return ((_info.start_x != -1) && (_info.start_y != -1));
    }
    bool checkMaskVisible(void) const;
    bool checkIndicatorBarVisible(Gesture::IndicatorBarType type) const;
    bool checkIndicatorBarScaleBackAnimRunning(Gesture::IndicatorBarType type) const
    {
        return _flags.is_indicator_bar_scale_back_anim_running[static_cast<int>(type)];
    }
    lv_obj_t *getEventObj(void) const
    {
        return _event_mask_obj.get();
    }
    lv_event_code_t getPressEventCode(void) const
    {
        return _press_event_code;
    }
    lv_event_code_t getPressingEventCode(void) const
    {
        return _pressing_event_code;
    }
    lv_event_code_t getReleaseEventCode(void) const
    {
        return _release_event_code;
    }
    int getIndicatorBarLength(Gesture::IndicatorBarType type) const;

    static bool calibrateData(const gui::StyleSize &screen_size, const base::Display &display,
                              Gesture::Data &data);

    base::Context &core;
    const Gesture::Data &data;

private:
    using IndicatorBarAnimVar_t = struct {
        Gesture *gesture;
        Gesture::IndicatorBarType type;
    };
    void resetGestureInfo(void);
    bool updateByNewData(void);

    static void onDataUpdateEventCallback(lv_event_t *event);
    static void onTouchDetectTimerCallback(struct _lv_timer_t *t);
    static void onIndicatorBarScaleBackAnimationExecuteCallback(void *var, int32_t value);
    static void onIndicatorBarScaleBackAnimationReadyCallback(lv_anim_t *anim);

    static constexpr Info GESTURE_INFO_INIT = {
        .direction = DIR_NONE,
        .start_area = AREA_CENTER,
        .stop_area = AREA_CENTER,
        .start_x = -1,
        .start_y = -1,
        .stop_x = -1,
        .stop_y = -1,
        .duration_ms = 0,
        .distance_px = 0,
        .flags = {
            .slow_speed = 0,
            .short_duration = 0,
        },
    };

    // Core
    lv_indev_t *_touch_device = nullptr;

    struct {
        std::array<bool, static_cast<int>(Gesture::IndicatorBarType::MAX)>  is_indicator_bar_scale_back_anim_running;
    } _flags = {};
    float _direction_tan_threshold = 0;
    std::array<int, static_cast<int>(Gesture::IndicatorBarType::MAX)>  _indicator_bar_min_lengths;
    std::array<int, static_cast<int>(Gesture::IndicatorBarType::MAX)>  _indicator_bar_max_lengths;
    uint32_t _touch_start_tick = 0;
    ESP_Brookesia_LvTimer_t _detect_timer;
    ESP_Brookesia_LvObj_t _event_mask_obj;
    std::array<ESP_Brookesia_LvObj_t, static_cast<int>(Gesture::IndicatorBarType::MAX)>  _indicator_bars;
    std::array<IndicatorBarAnimVar_t, static_cast<int>(Gesture::IndicatorBarType::MAX)>  _indicator_bar_anim_var;
    std::array<ESP_Brookesia_LvAnim_t, static_cast<int>(Gesture::IndicatorBarType::MAX)>  _indicator_bar_scale_back_anims;
    std::array<float, static_cast<int>(Gesture::IndicatorBarType::MAX)>  _indicator_bar_scale_factors;
    lv_event_code_t _press_event_code = LV_EVENT_ALL;
    lv_event_code_t _pressing_event_code = LV_EVENT_ALL;
    lv_event_code_t _release_event_code = LV_EVENT_ALL;
    Info _info = GESTURE_INFO_INIT;
    Info _event_data = GESTURE_INFO_INIT;
};

} // namespace esp_brookesia::systems::phone

using ESP_Brookesia_GestureDirection_t [[deprecated("Use `esp_brookesia::systems::phone::Direction` instead")]] =
    esp_brookesia::systems::phone::Gesture::Direction;
#define ESP_BROOKESIA_GESTURE_DIR_NONE  ESP_Brookesia_GestureDirection_t::DIR_NONE
#define ESP_BROOKESIA_GESTURE_DIR_UP    ESP_Brookesia_GestureDirection_t::DIR_UP
#define ESP_BROOKESIA_GESTURE_DIR_DOWN  ESP_Brookesia_GestureDirection_t::DIR_DOWN
#define ESP_BROOKESIA_GESTURE_DIR_LEFT  ESP_Brookesia_GestureDirection_t::DIR_LEFT
#define ESP_BROOKESIA_GESTURE_DIR_RIGHT ESP_Brookesia_GestureDirection_t::DIR_RIGHT
#define ESP_BROOKESIA_GESTURE_DIR_HOR   ESP_Brookesia_GestureDirection_t::DIR_HOR
#define ESP_BROOKESIA_GESTURE_DIR_VER   ESP_Brookesia_GestureDirection_t::DIR_VER
using ESP_Brookesia_GestureArea_t [[deprecated("Use `esp_brookesia::systems::phone::Area` instead")]] =
    esp_brookesia::systems::phone::Gesture::Area;
#define ESP_BROOKESIA_GESTURE_AREA_CENTER      ESP_Brookesia_GestureArea_t::AREA_CENTER
#define ESP_BROOKESIA_GESTURE_AREA_TOP_EDGE    ESP_Brookesia_GestureArea_t::AREA_TOP_EDGE
#define ESP_BROOKESIA_GESTURE_AREA_BOTTOM_EDGE ESP_Brookesia_GestureArea_t::AREA_BOTTOM_EDGE
#define ESP_BROOKESIA_GESTURE_AREA_LEFT_EDGE   ESP_Brookesia_GestureArea_t::AREA_LEFT_EDGE
#define ESP_BROOKESIA_GESTURE_AREA_RIGHT_EDGE  ESP_Brookesia_GestureArea_t::AREA_RIGHT_EDGE
using ESP_Brookesia_GestureIndicatorBarType_t [[deprecated("Use `esp_brookesia::systems::phone::Gesture::IndicatorBarType` instead")]] =
    esp_brookesia::systems::phone::Gesture::IndicatorBarType;
#define ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_LEFT   ESP_Brookesia_GestureIndicatorBarType_t::LEFT
#define ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_RIGHT  ESP_Brookesia_GestureIndicatorBarType_t::RIGHT
#define ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_BOTTOM ESP_Brookesia_GestureIndicatorBarType_t::BOTTOM
#define ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX    ESP_Brookesia_GestureIndicatorBarType_t::MAX
using ESP_Brookesia_GestureIndicatorBarData_t [[deprecated("Use `esp_brookesia::systems::phone::Gesture::IndicatorBarData` instead")]] =
    esp_brookesia::systems::phone::Gesture::IndicatorBarData;
using ESP_Brookesia_GestureData_t [[deprecated("Use `esp_brookesia::systems::phone::Gesture::Data` instead")]] =
    esp_brookesia::systems::phone::Gesture::Data;
using ESP_Brookesia_GestureInfo_t [[deprecated("Use `esp_brookesia::systems::phone::Gesture::Info` instead")]] =
    esp_brookesia::systems::phone::Gesture::Info;
using ESP_Brookesia_Gesture [[deprecated("Use `esp_brookesia::systems::phone::Gesture` instead")]] =
    esp_brookesia::systems::phone::Gesture;
