/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <unordered_map>
#include "systems/core/esp_brookesia_core.hpp"
#include "gui/lvgl/esp_brookesia_lv_helper.hpp"
#include "esp_brookesia_recents_screen_snapshot.hpp"

// *INDENT-OFF*

typedef struct {
    struct {
        int y_start;
        ESP_Brookesia_StyleSize_t size;
        int layout_row_pad;
        int layout_top_pad;
        int layout_bottom_pad;
        ESP_Brookesia_StyleColor_t background_color;
    } main;
    struct {
        ESP_Brookesia_StyleSize_t main_size;
        uint8_t main_layout_x_right_offset;
        ESP_Brookesia_StyleFont_t label_text_font;
        ESP_Brookesia_StyleColor_t label_text_color;
        const char *label_unit_text;
    } memory;
    struct {
        ESP_Brookesia_StyleSize_t main_size;
        int main_layout_column_pad;
        ESP_Brookesia_RecentsScreenSnapshotData_t snapshot;
    } snapshot_table;
    struct {
        ESP_Brookesia_StyleSize_t default_size;
        ESP_Brookesia_StyleSize_t press_size;
        ESP_Brookesia_StyleImage_t image;
    } trash_icon;
    struct {
        uint8_t enable_memory: 1;
        uint8_t enable_table_height_flex: 1;
        uint8_t enable_table_snapshot_use_icon_image: 1;
        uint8_t enable_table_scroll_anim: 1;
    } flags;
} ESP_Brookesia_RecentsScreenData_t;
class ESP_Brookesia_RecentsScreen {
public:
    ESP_Brookesia_RecentsScreen(ESP_Brookesia_Core &core, const ESP_Brookesia_RecentsScreenData_t &data);
    ~ESP_Brookesia_RecentsScreen();

    bool begin(lv_obj_t *parent);
    bool del(void);

    bool setVisible(bool visible) const;
    bool addSnapshot(const ESP_Brookesia_RecentsScreenSnapshotConf_t &conf);
    bool removeSnapshot(int id);
    bool scrollToSnapshotById(int id);
    bool scrollToSnapshotByIndex(uint8_t index);
    bool moveSnapshotY(int id, int y);
    bool updateSnapshotImage(int id);
    bool setMemoryLabel(int internal_free, int internal_total, int external_free, int external_total) const;

    bool checkInitialized(void) const   { return _main_obj != nullptr; }
    bool checkSnapshotExist(int id) const;
    bool checkVisible(void) const;
    bool checkPointInsideMain(lv_point_t &point) const;
    bool checkPointInsideTable(lv_point_t &point) const;
    bool checkPointInsideSnapshot(int id, lv_point_t &point) const;
    int getSnapshotOriginY(int id) const;
    int getSnapshotCurrentY(int id) const;
    int getSnapshotIdPointIn(lv_point_t &point) const;
    lv_obj_t *getEventObject(void) const { return _trash_icon.get(); }
    lv_event_code_t getSnapshotDeletedEventCode(void) const { return _snapshot_deleted_event_code; }
    int getSnapshotCount(void) const     { return (int)_id_snapshot_map.size(); }

    static bool calibrateData(const ESP_Brookesia_StyleSize_t &screen_size, const ESP_Brookesia_CoreHome &home,
                              ESP_Brookesia_RecentsScreenData_t &data);

private:
    bool updateByNewData(void);

    static void onDataUpdateEventCallback(lv_event_t *event);
    static void onTrashTouchEventCallback(lv_event_t *event);

    ESP_Brookesia_Core &_core;
    const ESP_Brookesia_RecentsScreenData_t &_data;

    bool _is_trash_pressed_losted;
    int _trash_icon_default_zoom;
    int _trash_icon_press_zoom;
    lv_event_code_t _snapshot_deleted_event_code;
    ESP_Brookesia_LvObj_t _main_obj;
    ESP_Brookesia_LvObj_t _memory_obj;
    ESP_Brookesia_LvObj_t _memory_label;
    ESP_Brookesia_LvObj_t _snapshot_table;
    ESP_Brookesia_LvObj_t _trash_obj;
    ESP_Brookesia_LvObj_t _trash_icon;
    std::unordered_map<int, std::shared_ptr<ESP_Brookesia_RecentsScreenSnapshot>> _id_snapshot_map;
};

// *INDENT-ON*
