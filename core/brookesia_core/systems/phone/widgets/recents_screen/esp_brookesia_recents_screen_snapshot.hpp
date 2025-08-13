/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/base/esp_brookesia_base_context.hpp"
#include "lvgl/esp_brookesia_lv_helper.hpp"

namespace esp_brookesia::systems::phone {

class RecentsScreenSnapshot {
public:
    struct Conf {
        const char *name;
        const void *icon_image_resource;
        const void *snapshot_image_resource;
        int id;
    };

    struct Data {
        gui::StyleSize main_size;
        struct {
            gui::StyleSize main_size;
            uint8_t main_layout_column_pad;
            gui::StyleSize icon_size;
            gui::StyleFont text_font;
            gui::StyleColor text_color;
        } title;
        struct {
            gui::StyleSize main_size;
            uint8_t radius;
        } image;
        struct {
            uint8_t enable_all_main_size_refer_screen: 1;
        } flags;
    };

    RecentsScreenSnapshot(const RecentsScreenSnapshot &) = delete;
    RecentsScreenSnapshot(RecentsScreenSnapshot &&) = delete;
    RecentsScreenSnapshot &operator=(const RecentsScreenSnapshot &) = delete;
    RecentsScreenSnapshot &operator=(RecentsScreenSnapshot &&) = delete;

    RecentsScreenSnapshot(base::Context &core, const Conf &conf, const Data &data);
    ~RecentsScreenSnapshot();

    bool begin(lv_obj_t *parent);
    bool del(void);

    bool checkInitialized(void) const
    {
        return (_main_obj != nullptr);
    }
    lv_obj_t *getMainObj(void) const
    {
        return _main_obj.get();
    }
    lv_obj_t *getDragObj(void) const
    {
        return _drag_obj.get();
    }
    int getOriginY(void) const
    {
        return _origin_y;
    }
    int getCurrentY(void) const;

    bool updateByNewData(void);

private:
    base::Context &_system_context;
    const Conf &_conf;
    const Data &_data;

    int _origin_y = 0;
    ESP_Brookesia_LvObj_t _main_obj;
    ESP_Brookesia_LvObj_t _drag_obj;
    ESP_Brookesia_LvObj_t _title_obj;
    ESP_Brookesia_LvObj_t _title_icon;
    ESP_Brookesia_LvObj_t _title_label;
    ESP_Brookesia_LvObj_t _snapshot_obj;
    ESP_Brookesia_LvObj_t _snapshot_image;
};

} // namespace esp_brookesia::systems::phone

using ESP_Brookesia_RecentsScreenSnapshotConf_t [[deprecated("Use `esp_brookesia::systems::phone::RecentsScreenSnapshot::Conf` instead")]] =
    esp_brookesia::systems::phone::RecentsScreenSnapshot::Conf;
using ESP_Brookesia_RecentsScreenSnapshotData_t [[deprecated("Use `esp_brookesia::systems::phone::RecentsScreenSnapshot::Data` instead")]] =
    esp_brookesia::systems::phone::RecentsScreenSnapshot::Data;
using ESP_Brookesia_RecentsScreenSnapshot [[deprecated("Use `esp_brookesia::systems::phone::RecentsScreenSnapshot` instead")]] =
    esp_brookesia::systems::phone::RecentsScreenSnapshot;
