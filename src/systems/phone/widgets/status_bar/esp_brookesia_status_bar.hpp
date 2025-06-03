/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <array>
#include <vector>
#include <map>
#include "systems/core/esp_brookesia_core.hpp"
#include "gui/lvgl/esp_brookesia_lv_helper.hpp"
#include "esp_brookesia_status_bar_icon.hpp"

// *INDENT-OFF*

#define ESP_BROOKESIA_STATUS_BAR_DATA_AREA_NUM_MAX             (3)

typedef enum {
    ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_UNKNOWN = 0,
    ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_START,
    ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_END,
    ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_CENTER,
    ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_MAX,
} ESP_Brookesia_StatusBarAreaAlign_t;

typedef struct {
    ESP_Brookesia_StyleSize_t size;
    ESP_Brookesia_StatusBarAreaAlign_t layout_column_align;
    int layout_column_start_offset;
    int layout_column_pad;
} ESP_Brookesia_StatusBarAreaData_t;

typedef struct {
    struct {
        ESP_Brookesia_StyleSize_t size;
        ESP_Brookesia_StyleSize_t size_min;
        ESP_Brookesia_StyleSize_t size_max;
        ESP_Brookesia_StyleColor_t background_color;
        ESP_Brookesia_StyleFont_t text_font;
        ESP_Brookesia_StyleColor_t text_color;
    } main;
    struct {
        uint8_t num;
        ESP_Brookesia_StatusBarAreaData_t data[ESP_BROOKESIA_STATUS_BAR_DATA_AREA_NUM_MAX];
    } area;
    ESP_Brookesia_StyleSize_t icon_common_size;
    struct {
        uint8_t area_index;
        ESP_Brookesia_StatusBarIconData_t icon_data;
    } battery;
    struct {
        uint8_t area_index;
        ESP_Brookesia_StatusBarIconData_t icon_data;
    } wifi;
    struct {
        uint8_t area_index;
    } clock;
    struct {
        uint8_t enable_main_size_min: 1;
        uint8_t enable_main_size_max: 1;
        uint32_t enable_battery_icon: 1;
        uint32_t enable_battery_icon_common_size: 1;
        uint32_t enable_battery_label: 1;
        uint32_t enable_wifi_icon: 1;
        uint32_t enable_wifi_icon_common_size: 1;
        uint32_t enable_clock: 1;
    } flags;
} ESP_Brookesia_StatusBarData_t;

typedef enum {
    ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_HIDE = 0,
    ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_SHOW_FIXED,
    ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_SHOW_FLEX,
    ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_MAX,
} ESP_Brookesia_StatusBarVisualMode_t;

class ESP_Brookesia_StatusBar {
public:
    enum class ClockFormat {
        FORMAT_12H,
        FORMAT_24H,
    };
    enum class WifiState {
        DISCONNECTED,
        SIGNAL_1,
        SIGNAL_2,
        SIGNAL_3,
    };

    ESP_Brookesia_StatusBar(const ESP_Brookesia_Core &core, const ESP_Brookesia_StatusBarData_t &data, int battery_id, int wifi_id);
    ~ESP_Brookesia_StatusBar();

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool setVisualMode(ESP_Brookesia_StatusBarVisualMode_t mode) const;
    bool addIcon(const ESP_Brookesia_StatusBarIconData_t &data, uint8_t area_index, int id);
    bool removeIcon(int id);
    bool setIconState(int id, int state) const;

    // Battery
    bool setBatteryPercent(bool charge_flag, int percent) const;
    bool showBatteryPercent(void) const;
    bool hideBatteryPercent(void) const;
    bool showBatteryIcon(void) const;
    bool hideBatteryIcon(void) const;
    // Wifi
    bool setWifiIconState(int state) const;
    bool setWifiIconState(WifiState state) const;
    // Clock
    bool setClockFormat(ClockFormat format) const;
    bool setClock(int hour, int min, bool is_pm) const;
    bool setClock(int hour, int min) const;

    bool checkVisible(void) const;

    static bool calibrateIconData(const ESP_Brookesia_StatusBarData_t &bar_data, const ESP_Brookesia_CoreHome &home,
                                  ESP_Brookesia_StatusBarIconData_t &icon_data);
    static bool calibrateData(const ESP_Brookesia_StyleSize_t &screen_size, const ESP_Brookesia_CoreHome &home,
                              ESP_Brookesia_StatusBarData_t &data);

private:
    bool beginMain(lv_obj_t *parent);
    bool updateMainByNewData(void);
    bool delMain(void);
    bool checkMainInitialized(void) const    { return (_main_obj != nullptr); }

    bool beginBattery(void);
    bool updateBatteryByNewData(void);
    bool delBattery(void);
    bool checkBatteryInitialized(void) const { return _is_battery_initialed; }

    bool beginWifi(void);

    bool beginClock(void);
    bool updateClockByNewData(void);
    bool delClock(void);
    bool checkClockInitialized(void) const   { return (_clock_obj != nullptr); }


    static void onDataUpdateEventCallback(lv_event_t *event);

    // Core
    const ESP_Brookesia_Core &_core;
    const ESP_Brookesia_StatusBarData_t &_data;
    // Main
    ESP_Brookesia_LvObj_t _main_obj;
    std::vector<ESP_Brookesia_LvObj_t> _area_objs;
    std::map <int, std::shared_ptr<ESP_Brookesia_StatusBarIcon>> _id_icon_map;
    // Battery
    int _battery_id;
    bool _is_battery_initialed;
    mutable int _battery_state;
    bool _is_battery_lable_out_of_area;
    ESP_Brookesia_LvObj_t _battery_label;
    // Wifi
    int _wifi_id;
    // Clock
    mutable int _clock_hour = 12;
    mutable int _clock_min = 0;
    mutable ClockFormat _clock_format = ClockFormat::FORMAT_24H;
    bool _is_clock_out_of_area;
    ESP_Brookesia_LvObj_t _clock_obj;
    ESP_Brookesia_LvObj_t _clock_hour_label;
    ESP_Brookesia_LvObj_t _clock_dot_label;
    ESP_Brookesia_LvObj_t _clock_min_label;
    ESP_Brookesia_LvObj_t _clock_period_label;
};

// *INDENT-ON*
