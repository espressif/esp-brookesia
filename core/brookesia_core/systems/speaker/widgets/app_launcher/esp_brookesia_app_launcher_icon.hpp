/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include <map>
#include "systems/base/esp_brookesia_base_context.hpp"

namespace esp_brookesia::systems::speaker {

typedef struct {
    const char *name;
    gui::StyleImage image;
    int id;
} AppLauncherIconInfo_t;

typedef struct {
    struct {
        gui::StyleSize size;
        uint8_t layout_row_pad;
    } main;
    struct {
        gui::StyleSize default_size;
        gui::StyleSize press_size;
    } image;
    struct {
        gui::StyleFont text_font;
        gui::StyleColor text_color;
    } label;
} AppLauncherIconData;

class AppLauncherIcon {
public:
    AppLauncherIcon(base::Context &core, const AppLauncherIconInfo_t &info, const AppLauncherIconData &data);
    ~AppLauncherIcon();

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool toggleClickable(bool clickable);

    bool checkInitialized(void) const
    {
        return (_main_obj != nullptr);
    }

    bool updateByNewData(void);

private:
    static void onIconTouchEventCallback(lv_event_t *event);

    base::Context &_system_context;
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

} // namespace esp_brookesia::systems::speaker
