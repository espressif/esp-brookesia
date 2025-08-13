/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "systems/base/esp_brookesia_base_context.hpp"

namespace esp_brookesia::systems::speaker {

enum GestureDirection {
    GESTURE_DIR_NONE  = 0,
    GESTURE_DIR_UP    = (1 << 0),
    GESTURE_DIR_DOWN  = (1 << 1),
    GESTURE_DIR_LEFT  = (1 << 2),
    GESTURE_DIR_RIGHT = (1 << 3),
    GESTURE_DIR_HOR   = (GESTURE_DIR_LEFT | GESTURE_DIR_RIGHT),
    GESTURE_DIR_VER   = (GESTURE_DIR_UP | GESTURE_DIR_DOWN),
};

enum GestureArea {
    GESTURE_AREA_CENTER      = 0,
    GESTURE_AREA_TOP_EDGE    = (1 << 0),
    GESTURE_AREA_BOTTOM_EDGE = (1 << 1),
    GESTURE_AREA_LEFT_EDGE   = (1 << 2),
    GESTURE_AREA_RIGHT_EDGE  = (1 << 3),
};

enum GestureIndicatorBarType {
    GESTURE_INDICATOR_BAR_TYPE_LEFT = 0,
    GESTURE_INDICATOR_BAR_TYPE_RIGHT,
    GESTURE_INDICATOR_BAR_TYPE_BOTTOM,
    GESTURE_INDICATOR_BAR_TYPE_MAX,
};

struct GestureIndicatorBarData {
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

struct GestureData {
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
    GestureIndicatorBarData indicator_bars[GESTURE_INDICATOR_BAR_TYPE_MAX];
    struct {
        uint8_t enable_indicator_bars[GESTURE_INDICATOR_BAR_TYPE_MAX];
    } flags;
};

struct GestureInfo {
    GestureDirection direction;
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

class Gesture {
public:
    Gesture(base::Context &core_in, const GestureData &data_in);
    ~Gesture();

    bool readTouchPoint(int &x, int &y) const;

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool setMaskObjectVisible(bool visible) const;
    bool setIndicatorBarLength(GestureIndicatorBarType type, int length) const;
    bool setIndicatorBarLengthByOffset(GestureIndicatorBarType type, int offset) const;
    bool setIndicatorBarVisible(GestureIndicatorBarType type, bool visible);
    bool controlIndicatorBarScaleBackAnim(GestureIndicatorBarType type, bool start);

    bool checkInitialized(void) const
    {
        return (_event_mask_obj != nullptr);
    }
    bool checkGestureStart(void) const
    {
        return ((_info.start_x != -1) && (_info.start_y != -1));
    }
    bool checkMaskVisible(void) const;
    bool checkIndicatorBarVisible(GestureIndicatorBarType type) const;
    bool checkIndicatorBarScaleBackAnimRunning(GestureIndicatorBarType type) const
    {
        return _flags.is_indicator_bar_scale_back_anim_running[type];
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
    int getIndicatorBarLength(GestureIndicatorBarType type) const;

    static bool calibrateData(const gui::StyleSize &screen_size, const base::Display &display,
                              GestureData &data);

    base::Context &core;
    const GestureData &data;

private:
    using IndicatorBarAnimVar_t = struct {
        Gesture *gesture;
        GestureIndicatorBarType type;
    };
    void resetGestureInfo(void);
    bool updateByNewData(void);

    static void onDataUpdateEventCallback(lv_event_t *event);
    static void onTouchDetectTimerCallback(struct _lv_timer_t *t);
    static void onIndicatorBarScaleBackAnimationExecuteCallback(void *var, int32_t value);
    static void onIndicatorBarScaleBackAnimationReadyCallback(lv_anim_t *anim);

    // Core
    lv_indev_t *_touch_device;

    struct {
        std::array<bool, GESTURE_INDICATOR_BAR_TYPE_MAX>  is_indicator_bar_scale_back_anim_running;
    } _flags;
    float _direction_tan_threshold;
    std::array<int, GESTURE_INDICATOR_BAR_TYPE_MAX>  _indicator_bar_min_lengths;
    std::array<int, GESTURE_INDICATOR_BAR_TYPE_MAX>  _indicator_bar_max_lengths;
    uint32_t _touch_start_tick;
    ESP_Brookesia_LvTimer_t _detect_timer;
    ESP_Brookesia_LvObj_t _event_mask_obj;
    std::array<ESP_Brookesia_LvObj_t, GESTURE_INDICATOR_BAR_TYPE_MAX>  _indicator_bars;
    std::array<IndicatorBarAnimVar_t, GESTURE_INDICATOR_BAR_TYPE_MAX>  _indicator_bar_anim_var;
    std::array<ESP_Brookesia_LvAnim_t, GESTURE_INDICATOR_BAR_TYPE_MAX>  _indicator_bar_scale_back_anims;
    std::array<float, GESTURE_INDICATOR_BAR_TYPE_MAX>  _indicator_bar_scale_factors;
    lv_event_code_t _press_event_code;
    lv_event_code_t _pressing_event_code;
    lv_event_code_t _release_event_code;
    GestureInfo _info;
    GestureInfo _event_data;
};

} // namespace esp_brookesia::systems::speaker
