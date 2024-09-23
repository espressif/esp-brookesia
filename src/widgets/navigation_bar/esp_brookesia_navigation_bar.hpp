/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include "lvgl.h"
#include "core/esp_brookesia_core.hpp"
#include "esp_brookesia_navigation_bar_type.h"
#include "esp_brookesia_navigation_bar.hpp"

// *INDENT-OFF*
class ESP_Brookesia_NavigationBar {
public:
    ESP_Brookesia_NavigationBar(const ESP_Brookesia_Core &core, const ESP_Brookesia_NavigationBarData_t &data);
    ~ESP_Brookesia_NavigationBar();

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool setVisualMode(ESP_Brookesia_NavigationBarVisualMode_t mode);
    bool triggerVisualFlexShow(void);
    bool show(void);
    bool hide(void);

    bool checkInitialized(void) const { return (_main_obj != nullptr); }
    bool checkVisible(void) const;
    bool checkVisualFlexShowAnimRunning(void) const  { return _flags.is_visual_flex_show_anim_running; }
    bool checkVisualFlexHideAnimRunning(void) const  { return _flags.is_visual_flex_hide_anim_running; }
    bool checkVisualFlexHideTimerRunning(void) const { return _flags.is_visual_flex_hide_timer_running; }
    const ESP_Brookesia_NavigationBarData_t &getData(void)  { return _data; }
    int getCurrentOffset(void) const;

    static bool calibrateData(const ESP_Brookesia_StyleSize_t &screen_size, const ESP_Brookesia_CoreHome &home,
                              ESP_Brookesia_NavigationBarData_t &data);

private:
    bool updateByNewData(void);
    bool startFlexShowAnimation(bool enable_auto_hide);
    bool stopFlexShowAnimation(void);
    bool startFlexHideAnimation(void);
    bool stopFlexHideAnimation(void);
    bool startFlexHideTimer(void);
    bool stopFlexHideTimer(void);
    bool resetFlexHideTimer(void);

    static void onDataUpdateEventCallback(lv_event_t *event);
    static void onIconTouchEventCallback(lv_event_t *event);
    static void onVisualFlexAnimationExecuteCallback(void *var, int32_t value);
    static void onVisualFlexShowAnimationReadyCallback(lv_anim_t *anim);
    static void onVisualFlexHideAnimationReadyCallback(lv_anim_t *anim);
    static void onVisualFlexHideTimerCallback(lv_timer_t *timer);

    const ESP_Brookesia_Core &_core;
    const ESP_Brookesia_NavigationBarData_t &_data;

    struct {
        uint8_t is_icon_pressed_losted: 1;
        uint8_t is_visual_flex_show_anim_running: 1;
        uint8_t is_visual_flex_hide_anim_running: 1;
        uint8_t is_visual_flex_hide_timer_running: 1;
        uint8_t enable_visual_flex_auto_hide: 1;
    } _flags;
    ESP_Brookesia_LvAnim_t _visual_flex_show_anim;
    ESP_Brookesia_LvAnim_t _visual_flex_hide_anim;
    ESP_Brookesia_LvTimer_t _visual_flex_hide_timer;
    ESP_Brookesia_NavigationBarVisualMode_t _visual_mode;
    ESP_Brookesia_LvObj_t _main_obj;
    std::vector<ESP_Brookesia_LvObj_t> _button_objs;
    std::vector<ESP_Brookesia_LvObj_t> _icon_main_objs;
    std::vector<ESP_Brookesia_LvObj_t> _icon_image_objs;
};
// *INDENT-OFF*
