/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include <map>
#include "systems/core/esp_brookesia_core.hpp"

// *INDENT-OFF*

namespace esp_brookesia::speaker {

typedef struct {
    const char *name;
    ESP_Brookesia_StyleImage_t image;
    int id;
} AppLauncherIconInfo_t;

typedef struct {
    struct {
        ESP_Brookesia_StyleSize_t size;
        uint8_t layout_row_pad;
    } main;
    struct {
        ESP_Brookesia_StyleSize_t default_size;
        ESP_Brookesia_StyleSize_t press_size;
    } image;
    struct {
        ESP_Brookesia_StyleFont_t text_font;
        ESP_Brookesia_StyleColor_t text_color;
    } label;
} AppLauncherIconData;

class AppLauncherIcon {
public:
    AppLauncherIcon(ESP_Brookesia_Core &core, const AppLauncherIconInfo_t &info, const AppLauncherIconData &data);
    ~AppLauncherIcon();

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool toggleClickable(bool clickable);

    bool checkInitialized(void) const { return (_main_obj != nullptr); }

    bool updateByNewData(void);

private:
    static void onIconTouchEventCallback(lv_event_t *event);

    ESP_Brookesia_Core &_core;
    AppLauncherIconInfo_t _info;
    const AppLauncherIconData &_data;

    struct {
        uint8_t is_pressed_losted: 1;
        uint8_t is_click_disable: 1;
    } _flags;
    int _image_default_zoom;
    int _image_press_zoom;
    ESP_Brookesia_LvObj_t _main_obj;
    ESP_Brookesia_LvObj_t _icon_main_obj;
    ESP_Brookesia_LvObj_t _icon_image_obj;
    ESP_Brookesia_LvObj_t _name_label;
};

} // namespace esp_brookesia::speaker

// *INDENT-ON*
