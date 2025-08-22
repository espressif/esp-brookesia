/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include <map>
#include "systems/base/esp_brookesia_base_context.hpp"
#include "lvgl/esp_brookesia_lv_helper.hpp"

namespace esp_brookesia::systems::phone {

class AppLauncherIcon {
public:
    struct Info {
        const char *name;
        gui::StyleImage image;
        int id;
    };

    struct Data {
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
    };

    AppLauncherIcon(base::Context &core, const Info &info, const Data &data);
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
    Info _info;
    const Data &_data;

    struct {
        uint8_t is_pressed_losted: 1;
        uint8_t is_click_disable: 1;
    } _flags;
    int _image_default_zoom;
    int _image_press_zoom;
    gui::LvObjSharedPtr _main_obj;
    gui::LvObjSharedPtr _icon_main_obj;
    gui::LvObjSharedPtr _icon_image_obj;
    gui::LvObjSharedPtr _name_label;
};

} // namespace esp_brookesia::systems::phone

using ESP_Brookesia_AppLauncherIconInfo_t [[deprecated("Use `esp_brookesia::systems::phone::AppLauncherIcon::Info` instead")]] =
    esp_brookesia::systems::phone::AppLauncherIcon::Info;
using ESP_Brookesia_AppLauncherIconData_t [[deprecated("Use `esp_brookesia::systems::phone::AppLauncherIcon::Data` instead")]] =
    esp_brookesia::systems::phone::AppLauncherIcon::Data;
using ESP_Brookesia_AppLauncherIcon [[deprecated("Use `esp_brookesia::systems::phone::AppLauncherIcon` instead")]] =
    esp_brookesia::systems::phone::AppLauncherIcon;
