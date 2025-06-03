/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include <map>
#include "systems/core/esp_brookesia_core.hpp"
#include "gui/lvgl/esp_brookesia_lv_helper.hpp"

// *INDENT-OFF*

#define ESP_BROOKESIA_STATUS_BAR_DATA_ICON_IMAGE_NUM_MAX       (6)

typedef struct {
    uint8_t image_num;
    ESP_Brookesia_StyleImage_t images[ESP_BROOKESIA_STATUS_BAR_DATA_ICON_IMAGE_NUM_MAX];
} ESP_Brookesia_StatusBarIconImage_t;

typedef struct {
    ESP_Brookesia_StyleSize_t size;
    ESP_Brookesia_StatusBarIconImage_t icon;
} ESP_Brookesia_StatusBarIconData_t;

class ESP_Brookesia_StatusBarIcon {
public:
    ESP_Brookesia_StatusBarIcon(const ESP_Brookesia_StatusBarIconData_t &data);
    ~ESP_Brookesia_StatusBarIcon();

    bool begin(const ESP_Brookesia_Core &core, lv_obj_t *parent);
    bool del(void);
    bool setCurrentState(int state);

    bool checkInitialized(void) const { return (_main_obj != nullptr); }

    bool updateByNewData(void);

private:
    const ESP_Brookesia_StatusBarIconData_t &_data;

    bool _is_out_of_parent;
    int _current_state;
    ESP_Brookesia_LvObj_t _main_obj;
    std::vector<ESP_Brookesia_LvObj_t> _image_objs;
};

// *INDENT-ON*
