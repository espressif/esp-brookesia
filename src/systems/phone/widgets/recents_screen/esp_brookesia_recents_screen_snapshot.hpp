/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/core/esp_brookesia_core.hpp"
#include "gui/lvgl/esp_brookesia_lv_helper.hpp"

// *INDENT-OFF*

typedef struct {
    const char *name;
    const void *icon_image_resource;
    const void *snapshot_image_resource;
    int id;
} ESP_Brookesia_RecentsScreenSnapshotConf_t;

typedef struct {
    ESP_Brookesia_StyleSize_t main_size;
    struct {
        ESP_Brookesia_StyleSize_t main_size;
        uint8_t main_layout_column_pad;
        ESP_Brookesia_StyleSize_t icon_size;
        ESP_Brookesia_StyleFont_t text_font;
        ESP_Brookesia_StyleColor_t text_color;
    } title;
    struct {
        ESP_Brookesia_StyleSize_t main_size;
        uint8_t radius;
    } image;
    struct {
        uint8_t enable_all_main_size_refer_screen: 1;
    } flags;
} ESP_Brookesia_RecentsScreenSnapshotData_t;

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

// *INDENT-ON*
