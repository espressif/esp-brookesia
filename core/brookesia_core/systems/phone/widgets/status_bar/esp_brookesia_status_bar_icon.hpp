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

class StatusBarIcon {
public:
    static constexpr int IMAGE_NUM_MAX = 6;

    struct Image {
        uint8_t image_num;
        gui::StyleImage images[IMAGE_NUM_MAX];
    };

    struct Data {
        gui::StyleSize size;
        Image icon;
    };

    StatusBarIcon(const StatusBarIcon &) = delete;
    StatusBarIcon(StatusBarIcon &&) = delete;
    StatusBarIcon &operator=(const StatusBarIcon &) = delete;
    StatusBarIcon &operator=(StatusBarIcon &&) = delete;

    StatusBarIcon(const Data &data);
    ~StatusBarIcon();

    bool begin(base::Context &core, lv_obj_t *parent);
    bool del(void);
    bool setCurrentState(int state);

    bool checkInitialized(void) const
    {
        return (_main_obj != nullptr);
    }

    bool updateByNewData(void);

private:
    const Data &_data;

    bool _is_out_of_parent = false;
    int _current_state = 0;
    ESP_Brookesia_LvObj_t _main_obj;
    std::vector<ESP_Brookesia_LvObj_t> _image_objs;
};

} // namespace esp_brookesia::systems::phone

using ESP_Brookesia_StatusBarIconData_t [[deprecated("Use `esp_brookesia::systems::phone::StatusBarIcon::Data` instead")]] =
    esp_brookesia::systems::phone::StatusBarIcon::Data;
using ESP_Brookesia_StatusBarIcon [[deprecated("Use `esp_brookesia::systems::phone::StatusBarIcon` instead")]] =
    esp_brookesia::systems::phone::StatusBarIcon;
#define ESP_BROOKESIA_STATUS_BAR_DATA_ICON_IMAGE_NUM_MAX ESP_Brookesia_StatusBarIcon::IMAGE_NUM_MAX
