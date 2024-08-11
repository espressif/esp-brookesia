/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include "lvgl.h"
#include "core/esp_ui_core.hpp"
#include "esp_ui_navigation_bar_type.h"
#include "esp_ui_navigation_bar.hpp"

// *INDENT-OFF*
class ESP_UI_NavigationBar {
public:
    ESP_UI_NavigationBar(const ESP_UI_Core &core, const ESP_UI_NavigationBarData_t &data);
    ~ESP_UI_NavigationBar();

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool setVisible(bool visible) const;

    bool checkInitialized(void) const { return (_main_obj != nullptr); }
    bool checkVisible(void) const;

    static bool calibrateData(const ESP_UI_StyleSize_t &screen_size, const ESP_UI_CoreHome &home,
                              ESP_UI_NavigationBarData_t &data);

private:
    bool updateByNewData(void);

    static void onDataUpdateEventCallback(lv_event_t *event);
    static void onIconTouchEventCallback(lv_event_t *event);

    const ESP_UI_Core &_core;
    const ESP_UI_NavigationBarData_t &_data;

    bool _is_icon_pressed_losted;
    ESP_UI_LvObj_t _main_obj;
    std::vector<ESP_UI_LvObj_t> _button_objs;
    std::vector<ESP_UI_LvObj_t> _icon_main_objs;
    std::vector<ESP_UI_LvObj_t> _icon_image_objs;
};
// *INDENT-OFF*
