/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include <map>
#include "lvgl.h"
#include "core/esp_ui_core.hpp"
#include "esp_ui_app_launcher_type.h"

// *INDENT-OFF*
class ESP_UI_AppLauncherIcon {
public:
    ESP_UI_AppLauncherIcon(ESP_UI_Core &core, const ESP_UI_AppLauncherIconInfo_t &info, const ESP_UI_AppLauncherIconData_t &data);
    ~ESP_UI_AppLauncherIcon();

    bool begin(lv_obj_t *parent);
    bool del(void);

    bool checkInitialized(void) const { return (_main_obj != nullptr); }

    bool updateByNewData(void);

private:
    static void onIconTouchEventCallback(lv_event_t *event);

    ESP_UI_Core &_core;
    ESP_UI_AppLauncherIconInfo_t _info;
    const ESP_UI_AppLauncherIconData_t &_data;

    bool _is_pressed_losted;
    uint16_t _image_default_zoom;
    uint16_t _image_press_zoom;
    ESP_UI_LvObj_t _main_obj;
    ESP_UI_LvObj_t _icon_main_obj;
    ESP_UI_LvObj_t _icon_image_obj;
    ESP_UI_LvObj_t _name_label;
};
// *INDENT-OFF*
