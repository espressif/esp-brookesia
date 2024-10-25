/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "core/esp_brookesia_core.hpp"
#include "esp_brookesia_app_launcher_type.h"
#include "esp_brookesia_app_launcher_icon.hpp"

// *INDENT-OFF*
class ESP_Brookesia_AppLauncher {
public:
    ESP_Brookesia_AppLauncher(ESP_Brookesia_Core &core, const ESP_Brookesia_AppLauncherData_t &data);
    ~ESP_Brookesia_AppLauncher();

    bool begin(lv_obj_t *parent);
    bool del(void);

    bool addIcon(uint8_t page_index, const ESP_Brookesia_AppLauncherIconInfo_t &info);
    bool removeIcon(int id);
    bool changeIconTable(int id, uint8_t new_table_index);
    bool scrollToPage(uint8_t index);
    bool scrollToRightPage(void);
    bool scrollToLeftPage(void);

    bool checkInitialized(void) const        { return (_main_obj != nullptr); }
    bool checkTableFull(uint8_t page_index) const;
    bool checkVisible(void) const;
    bool checkPointInsideMain(lv_point_t &point) const;
    uint8_t getActiveScreenIndex(void) const { return _table_current_page_index; }

    static bool calibrateData(const ESP_Brookesia_StyleSize_t &screen_size, const ESP_Brookesia_CoreHome &home,
                              ESP_Brookesia_AppLauncherData_t &data);

private:
    typedef struct {
        uint8_t page_icon_count;
        ESP_Brookesia_LvObj_t page_main_obj;
        ESP_Brookesia_LvObj_t page_obj;
        ESP_Brookesia_LvObj_t spot_obj;
    } ESP_Brookesia_AppLauncherMixObject_t;
    typedef struct {
        uint8_t current_page_index;
        uint8_t target_page_index;
        std::shared_ptr<ESP_Brookesia_AppLauncherIcon> icon;
    } ESP_Brookesia_AppLauncherMixIcon_t;

    bool createMixObject(ESP_Brookesia_LvObj_t &table_obj, ESP_Brookesia_LvObj_t &indicator_obj,
                         std::vector<ESP_Brookesia_AppLauncherMixObject_t> &mix_objs);
    bool destoryMixObject(uint8_t index, std::vector<ESP_Brookesia_AppLauncherMixObject_t> &mix_objs);
    bool updateMixByNewData(uint8_t index, std::vector<ESP_Brookesia_AppLauncherMixObject_t> &mix_objs);
    bool togglePageIconClickable(uint8_t page_index, bool clickable);
    bool toggleCurrentPageIconClickable(bool clickable);
    bool updateActiveSpot(void);
    bool updateByNewData(void);

    static void onDataUpdateEventCallback(lv_event_t *event);
    static void onPageTouchEventCallback(lv_event_t *event);

    // Core
    ESP_Brookesia_Core &_core;
    const ESP_Brookesia_AppLauncherData_t &_data;

    int _table_current_page_index;
    uint8_t _table_page_icon_count_max;
    uint16_t _table_page_pad_row;
    uint16_t _table_page_pad_column;
    ESP_Brookesia_LvObj_t _main_obj;
    ESP_Brookesia_LvObj_t _table_obj;
    ESP_Brookesia_LvObj_t _indicator_obj;
    std::vector <ESP_Brookesia_AppLauncherMixObject_t> _mix_objs;
    std::map <int, ESP_Brookesia_AppLauncherMixIcon_t> _id_mix_icon_map;
};
// *INDENT-OFF*
