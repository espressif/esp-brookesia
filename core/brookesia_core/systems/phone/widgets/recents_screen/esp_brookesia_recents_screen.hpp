/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <unordered_map>
#include "systems/base/esp_brookesia_base_context.hpp"
#include "lvgl/esp_brookesia_lv_helper.hpp"
#include "esp_brookesia_recents_screen_snapshot.hpp"

namespace esp_brookesia::systems::phone {

class RecentsScreen {
public:
    struct Data {
        struct {
            int y_start;
            gui::StyleSize size;
            int layout_row_pad;
            int layout_top_pad;
            int layout_bottom_pad;
            gui::StyleColor background_color;
        } main;
        struct {
            gui::StyleSize main_size;
            uint8_t main_layout_x_right_offset;
            gui::StyleFont label_text_font;
            gui::StyleColor label_text_color;
            const char *label_unit_text;
        } memory;
        struct {
            gui::StyleSize main_size;
            int main_layout_column_pad;
            RecentsScreenSnapshot::Data snapshot;
        } snapshot_table;
        struct {
            gui::StyleSize default_size;
            gui::StyleSize press_size;
            gui::StyleImage image;
        } trash_icon;
        struct {
            uint8_t enable_memory: 1;
            uint8_t enable_table_height_flex: 1;
            uint8_t enable_table_snapshot_use_icon_image: 1;
            uint8_t enable_table_scroll_anim: 1;
        } flags;
    };

    RecentsScreen(const RecentsScreen &) = delete;
    RecentsScreen(RecentsScreen &&) = delete;
    RecentsScreen &operator=(const RecentsScreen &) = delete;
    RecentsScreen &operator=(RecentsScreen &&) = delete;

    RecentsScreen(base::Context &core, const Data &data);
    ~RecentsScreen();

    bool begin(lv_obj_t *parent);
    bool del(void);

    bool setVisible(bool visible) const;
    bool addSnapshot(const RecentsScreenSnapshot::Conf &conf);
    bool removeSnapshot(int id);
    bool scrollToSnapshotById(int id);
    bool scrollToSnapshotByIndex(uint8_t index);
    bool moveSnapshotY(int id, int y);
    bool updateSnapshotImage(int id);
    bool setMemoryLabel(int internal_free, int internal_total, int external_free, int external_total) const;

    bool checkInitialized(void) const
    {
        return _main_obj != nullptr;
    }
    bool checkSnapshotExist(int id) const;
    bool checkVisible(void) const;
    bool checkPointInsideMain(lv_point_t &point) const;
    bool checkPointInsideTable(lv_point_t &point) const;
    bool checkPointInsideSnapshot(int id, lv_point_t &point) const;
    int getSnapshotOriginY(int id) const;
    int getSnapshotCurrentY(int id) const;
    int getSnapshotIdPointIn(lv_point_t &point) const;
    lv_obj_t *getEventObject(void) const
    {
        return _trash_icon.get();
    }
    lv_event_code_t getSnapshotDeletedEventCode(void) const
    {
        return _snapshot_deleted_event_code;
    }
    int getSnapshotCount(void) const
    {
        return (int)_id_snapshot_map.size();
    }

    static bool calibrateData(const gui::StyleSize &screen_size, const base::Display &display, Data &data);

private:
    bool updateByNewData(void);

    static void onDataUpdateEventCallback(lv_event_t *event);
    static void onTrashTouchEventCallback(lv_event_t *event);

    base::Context &_system_context;
    const Data &_data;

    bool _is_trash_pressed_losted = false;
    int _trash_icon_default_zoom = LV_SCALE_NONE;
    int _trash_icon_press_zoom = LV_SCALE_NONE;
    lv_event_code_t _snapshot_deleted_event_code = LV_EVENT_ALL;
    ESP_Brookesia_LvObj_t _main_obj;
    ESP_Brookesia_LvObj_t _memory_obj;
    ESP_Brookesia_LvObj_t _memory_label;
    ESP_Brookesia_LvObj_t _snapshot_table;
    ESP_Brookesia_LvObj_t _trash_obj;
    ESP_Brookesia_LvObj_t _trash_icon;
    std::unordered_map<int, std::shared_ptr<RecentsScreenSnapshot>> _id_snapshot_map;
};

} // namespace esp_brookesia::systems::phone

using ESP_Brookesia_RecentsScreenData_t [[deprecated("Use `esp_brookesia::systems::phone::RecentsScreen::Data` instead")]] =
    esp_brookesia::systems::phone::RecentsScreen::Data;
using ESP_Brookesia_RecentsScreen [[deprecated("Use `esp_brookesia::systems::phone::RecentsScreen` instead")]] =
    esp_brookesia::systems::phone::RecentsScreen;
