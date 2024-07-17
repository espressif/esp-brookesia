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
#include "esp_ui_status_bar_type.h"

// *INDENT-OFF*
class ESP_UI_StatusBarIcon {
public:
    ESP_UI_StatusBarIcon(const ESP_UI_StatusBarIconData_t &data);
    ~ESP_UI_StatusBarIcon();

    bool begin(const ESP_UI_Core &core, lv_obj_t *parent);
    bool del(void);
    bool setCurrentState(int state);

    bool checkInitialized(void) const { return (_main_obj != nullptr); }

    bool updateByNewData(void);

private:
    const ESP_UI_StatusBarIconData_t &_data;

    bool _is_out_of_parent;
    int _current_state;
    ESP_UI_LvObj_t _main_obj;
    std::vector<ESP_UI_LvObj_t> _image_objs;
};
// *INDENT-OFF*
