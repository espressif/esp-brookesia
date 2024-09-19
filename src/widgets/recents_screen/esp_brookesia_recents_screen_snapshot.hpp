/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "core/esp_brookesia_core.hpp"
#include "esp_brookesia_recents_screen_type.h"

// *INDENT-OFF*
class ESP_Brookesia_RecentsScreenSnapshot {
public:
    ESP_Brookesia_RecentsScreenSnapshot(const ESP_Brookesia_Core &core, const ESP_Brookesia_RecentsScreenSnapshotConf_t &conf,
                                 const ESP_Brookesia_RecentsScreenSnapshotData_t &data);
    ~ESP_Brookesia_RecentsScreenSnapshot();

    bool begin(lv_obj_t *parent);
    bool del(void);

    bool checkInitialized(void) const { return (_main_obj != nullptr); }
    lv_obj_t *getMainObj(void) const  { return _main_obj.get(); }
    lv_obj_t *getDragObj(void) const  { return _drag_obj.get(); }
    int getOriginY(void) const        { return _origin_y; }
    int getCurrentY(void) const;

    bool updateByNewData(void);

private:
    const ESP_Brookesia_Core &_core;
    const ESP_Brookesia_RecentsScreenSnapshotConf_t &_conf;
    const ESP_Brookesia_RecentsScreenSnapshotData_t &_data;

    int _origin_y;
    ESP_Brookesia_LvObj_t _main_obj;
    ESP_Brookesia_LvObj_t _drag_obj;
    ESP_Brookesia_LvObj_t _title_obj;
    ESP_Brookesia_LvObj_t _title_icon;
    ESP_Brookesia_LvObj_t _title_label;
    ESP_Brookesia_LvObj_t _snapshot_obj;
    ESP_Brookesia_LvObj_t _snapshot_image;
};
// *INDENT-OFF*
