/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "core/esp_ui_core_type.h"
#include "core/esp_ui_core.hpp"
#include "esp_ui_gesture_type.h"

// *INDENT-OFF*
class ESP_UI_Gesture {
public:
    ESP_UI_Gesture(ESP_UI_Core &core, const ESP_UI_GestureData_t &data);
    ~ESP_UI_Gesture();

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool enableMaskObject(bool enable);
    bool readTouchPoint(int &x, int &y) const;

    bool checkInitialized(void) const                { return (_event_mask_obj != nullptr); }
    bool checkGestureStart(void) const               { return ((_info.start_x != -1) && (_info.start_y != -1)); }
    lv_obj_t *getEventObj(void) const                { return _event_mask_obj.get(); }
    lv_event_code_t getPressEventCode(void) const    { return _press_event_code; }
    lv_event_code_t getPressingEventCode(void) const { return _pressing_event_code; }
    lv_event_code_t getReleaseEventCode(void) const  { return _release_event_code; }

    static bool calibrateData(const ESP_UI_CoreData_t &core_data, ESP_UI_GestureData_t &data);

private:
    bool setMaskObjectVisible(bool visible) const;
    void resetGestureInfo(void);
    bool updateByNewData(void);

    static void onDataUpdateEventCallback(lv_event_t *event);
    static void onTouchDetectTimerCallback(struct _lv_timer_t *t);

    // Core
    ESP_UI_Core &_core;
    const ESP_UI_GestureData_t &_data;
    lv_indev_t *_touch_device;

    bool _enable_mask_object;
    float _direction_tan_threshold;
    uint32_t _touch_start_tick;
    ESP_UI_LvTimer_t _detect_timer;
    ESP_UI_LvObj_t _event_mask_obj;
    lv_event_code_t _press_event_code;
    lv_event_code_t _pressing_event_code;
    lv_event_code_t _release_event_code;
    ESP_UI_GestureInfo_t _info;
    ESP_UI_GestureInfo_t _event_data;
};
// *INDENT-OFF*
