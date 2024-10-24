/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include <map>
#include "lvgl.h"
#include "core/esp_brookesia_core.hpp"
#include "esp_brookesia_app_launcher_type.h"

// *INDENT-OFF*
class ESP_Brookesia_AppLauncherIcon {
public:
    ESP_Brookesia_AppLauncherIcon(ESP_Brookesia_Core &core, const ESP_Brookesia_AppLauncherIconInfo_t &info, const ESP_Brookesia_AppLauncherIconData_t &data);
    ~ESP_Brookesia_AppLauncherIcon();

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool toggleClickable(bool clickable);

    bool checkInitialized(void) const { return (_main_obj != nullptr); }

    bool updateByNewData(void);

private:
    static void onIconTouchEventCallback(lv_event_t *event);

    ESP_Brookesia_Core &_core;
    ESP_Brookesia_AppLauncherIconInfo_t _info;
    const ESP_Brookesia_AppLauncherIconData_t &_data;

    struct {
        uint8_t is_pressed_losted: 1;
        uint8_t is_click_disable: 1;
    } _flags;
    uint16_t _image_default_zoom;
    uint16_t _image_press_zoom;
    ESP_Brookesia_LvObj_t _main_obj;
    ESP_Brookesia_LvObj_t _icon_main_obj;
    ESP_Brookesia_LvObj_t _icon_image_obj;
    ESP_Brookesia_LvObj_t _name_label;
};
// *INDENT-OFF*
