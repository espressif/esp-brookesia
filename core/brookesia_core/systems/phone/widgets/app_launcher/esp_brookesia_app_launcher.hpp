/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/base/esp_brookesia_base_context.hpp"
#include "lvgl/esp_brookesia_lv_helper.hpp"
#include "esp_brookesia_app_launcher_icon.hpp"

namespace esp_brookesia::systems::phone {

struct AppLauncherData {
    struct {
        int y_start;
        gui::StyleSize size;
    } main;
    struct {
        uint8_t default_num;
        gui::StyleSize size;
    } table;
    struct {
        gui::StyleSize main_size;
        uint8_t main_layout_column_pad;
        int main_layout_bottom_offset;
        gui::StyleSize spot_inactive_size;
        gui::StyleSize spot_active_size;
        gui::StyleColor spot_inactive_background_color;
        gui::StyleColor spot_active_background_color;
    } indicator;
    AppLauncherIcon::Data icon;
    struct {
        uint8_t enable_table_scroll_anim: 1;
    } flags;
};

class AppLauncher {
public:
    AppLauncher(base::Context &core, const AppLauncherData &data);
    ~AppLauncher();

    bool begin(lv_obj_t *parent);
    bool del(void);

    bool addIcon(uint8_t page_index, const AppLauncherIcon::Info &info);
    bool removeIcon(int id);
    bool changeIconTable(int id, uint8_t new_table_index);
    bool scrollToPage(uint8_t index);
    bool scrollToRightPage(void);
    bool scrollToLeftPage(void);

    bool checkInitialized(void) const
    {
        return (_main_obj != nullptr);
    }
    bool checkTableFull(uint8_t page_index) const;
    bool checkVisible(void) const;
    bool checkPointInsideMain(lv_point_t &point) const;
    uint8_t getActiveScreenIndex(void) const
    {
        return _table_current_page_index;
    }

    static bool calibrateData(const gui::StyleSize &screen_size, const base::Display &display, AppLauncherData &data);

private:
    struct MixObject {
        uint8_t page_icon_count;
        gui::LvObjSharedPtr page_main_obj;
        gui::LvObjSharedPtr page_obj;
        gui::LvObjSharedPtr spot_obj;
    };
    struct MixIcon {
        uint8_t current_page_index;
        uint8_t target_page_index;
        std::shared_ptr<AppLauncherIcon> icon;
    };

    bool createMixObject(gui::LvObjSharedPtr &table_obj, gui::LvObjSharedPtr &indicator_obj,
                         std::vector<MixObject> &mix_objs);
    bool destoryMixObject(uint8_t index, std::vector<MixObject> &mix_objs);
    bool updateMixByNewData(uint8_t index, std::vector<MixObject> &mix_objs);
    bool togglePageIconClickable(uint8_t page_index, bool clickable);
    bool toggleCurrentPageIconClickable(bool clickable);
    bool updateActiveSpot(void);
    bool updateByNewData(void);

    static void onDataUpdateEventCallback(lv_event_t *event);
    static void onPageTouchEventCallback(lv_event_t *event);

    // Core
    base::Context &_system_context;
    const AppLauncherData &_data;

    int _table_current_page_index;
    uint8_t _table_page_icon_count_max;
    int _table_page_pad_row;
    int _table_page_pad_column;
    gui::LvObjSharedPtr _main_obj;
    gui::LvObjSharedPtr _table_obj;
    gui::LvObjSharedPtr _indicator_obj;
    std::vector <MixObject> _mix_objs;
    std::map <int, MixIcon> _id_mix_icon_map;
};

} // namespace esp_brookesia::systems::phone

using ESP_Brookesia_AppLauncherData_t [[deprecated("Use `esp_brookesia::systems::phone::AppLauncherData` instead")]] =
    esp_brookesia::systems::phone::AppLauncherData;
using ESP_Brookesia_AppLauncher [[deprecated("Use `esp_brookesia::systems::phone::AppLauncher` instead")]] =
    esp_brookesia::systems::phone::AppLauncher;
