/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "core/esp_brookesia_core_type.h"
#include "core/esp_brookesia_core.hpp"
#include "esp_brookesia_gesture_type.h"

// *INDENT-OFF*
class ESP_Brookesia_Gesture {
public:
    ESP_Brookesia_Gesture(ESP_Brookesia_Core &core_in, const ESP_Brookesia_GestureData_t &data_in);
    ~ESP_Brookesia_Gesture();

    bool readTouchPoint(int &x, int &y) const;

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool setMaskObjectVisible(bool visible) const;
    bool setIndicatorBarLength(ESP_Brookesia_GestureIndicatorBarType_t type, uint16_t length) const;
    bool setIndicatorBarLengthByOffset(ESP_Brookesia_GestureIndicatorBarType_t type, int offset) const;
    bool setIndicatorBarVisible(ESP_Brookesia_GestureIndicatorBarType_t type, bool visible);
    bool controlIndicatorBarScaleBackAnim(ESP_Brookesia_GestureIndicatorBarType_t type, bool start);

    bool checkInitialized(void) const                { return (_event_mask_obj != nullptr); }
    bool checkGestureStart(void) const               { return ((_info.start_x != -1) && (_info.start_y != -1)); }
    bool checkMaskVisible(void) const;
    bool checkIndicatorBarVisible(ESP_Brookesia_GestureIndicatorBarType_t type) const;
    bool checkIndicatorBarScaleBackAnimRunning(ESP_Brookesia_GestureIndicatorBarType_t type) const
                                                     { return _flags.is_indicator_bar_scale_back_anim_running[type]; }
    lv_obj_t *getEventObj(void) const                { return _event_mask_obj.get(); }
    lv_event_code_t getPressEventCode(void) const    { return _press_event_code; }
    lv_event_code_t getPressingEventCode(void) const { return _pressing_event_code; }
    lv_event_code_t getReleaseEventCode(void) const  { return _release_event_code; }
    int getIndicatorBarLength(ESP_Brookesia_GestureIndicatorBarType_t type) const;

    static bool calibrateData(const ESP_Brookesia_StyleSize_t &screen_size, const ESP_Brookesia_CoreHome &home,
                              ESP_Brookesia_GestureData_t &data);

    ESP_Brookesia_Core &core;
    const ESP_Brookesia_GestureData_t &data;

private:
    using IndicatorBarAnimVar_t = struct {
        ESP_Brookesia_Gesture *gesture;
        ESP_Brookesia_GestureIndicatorBarType_t type;
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
        std::array<bool, ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX>  is_indicator_bar_scale_back_anim_running;
    } _flags;
    float _direction_tan_threshold;
    std::array<int, ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX>  _indicator_bar_min_lengths;
    std::array<int, ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX>  _indicator_bar_max_lengths;
    uint32_t _touch_start_tick;
    ESP_Brookesia_LvTimer_t _detect_timer;
    ESP_Brookesia_LvObj_t _event_mask_obj;
    std::array<ESP_Brookesia_LvObj_t, ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX>  _indicator_bars;
    std::array<IndicatorBarAnimVar_t, ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX>  _indicator_bar_anim_var;
    std::array<ESP_Brookesia_LvAnim_t, ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX>  _indicator_bar_scale_back_anims;
    std::array<float, ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX>  _indicator_bar_scale_factors;
    lv_event_code_t _press_event_code;
    lv_event_code_t _pressing_event_code;
    lv_event_code_t _release_event_code;
    ESP_Brookesia_GestureInfo_t _info;
    ESP_Brookesia_GestureInfo_t _event_data;
};
// *INDENT-OFF*
