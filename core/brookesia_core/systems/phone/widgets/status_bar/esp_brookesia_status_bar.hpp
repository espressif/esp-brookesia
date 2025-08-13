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
#include "systems/base/esp_brookesia_base_context.hpp"
#include "lvgl/esp_brookesia_lv_helper.hpp"
#include "esp_brookesia_status_bar_icon.hpp"

namespace esp_brookesia::systems::phone {

class StatusBar {
public:
    static constexpr int AREA_NUM_MAX = 3;

    enum class AreaAlign {
        UNKNOWN = 0,
        START,
        END,
        CENTER,
        MAX,
    };

    struct AreaData {
        gui::StyleSize size;
        AreaAlign layout_column_align;
        int layout_column_start_offset;
        int layout_column_pad;
    };

    struct Data {
        struct {
            gui::StyleSize size;
            gui::StyleSize size_min;
            gui::StyleSize size_max;
            gui::StyleColor background_color;
            gui::StyleFont text_font;
            gui::StyleColor text_color;
        } main;
        struct {
            uint8_t num;
            AreaData data[AREA_NUM_MAX];
        } area;
        gui::StyleSize icon_common_size;
        struct {
            uint8_t area_index;
            StatusBarIcon::Data icon_data;
        } battery;
        struct {
            uint8_t area_index;
            StatusBarIcon::Data icon_data;
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
    };

    enum class VisualMode {
        HIDE = 0,
        SHOW_FIXED,
        SHOW_FLEX,
        MAX,
    };

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

    StatusBar(base::Context &core, const Data &data, int battery_id, int wifi_id);
    ~StatusBar();

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool setVisualMode(VisualMode mode) const;
    bool addIcon(const StatusBarIcon::Data &data, uint8_t area_index, int id);
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

    static bool calibrateIconData(const Data &bar_data, const base::Display &display,
                                  StatusBarIcon::Data &icon_data);
    static bool calibrateData(const gui::StyleSize &screen_size, const base::Display &display,
                              Data &data);

private:
    bool beginMain(lv_obj_t *parent);
    bool updateMainByNewData(void);
    bool delMain(void);
    bool checkMainInitialized(void) const
    {
        return (_main_obj != nullptr);
    }

    bool beginBattery(void);
    bool updateBatteryByNewData(void);
    bool delBattery(void);
    bool checkBatteryInitialized(void) const
    {
        return _is_battery_initialed;
    }

    bool beginWifi(void);

    bool beginClock(void);
    bool updateClockByNewData(void);
    bool delClock(void);
    bool checkClockInitialized(void) const
    {
        return (_clock_obj != nullptr);
    }


    static void onDataUpdateEventCallback(lv_event_t *event);

    // Core
    base::Context &_system_context;
    const Data &_data;
    // Main
    ESP_Brookesia_LvObj_t _main_obj;
    std::vector<ESP_Brookesia_LvObj_t> _area_objs;
    std::map <int, std::shared_ptr<StatusBarIcon>> _id_icon_map;
    // Battery
    int _battery_id = -1;
    bool _is_battery_initialed = false;
    mutable int _battery_state = -1;
    bool _is_battery_lable_out_of_area = false;
    ESP_Brookesia_LvObj_t _battery_label;
    // Wifi
    int _wifi_id = -1;
    // Clock
    mutable int _clock_hour = -1;
    mutable int _clock_min = -1;
    mutable ClockFormat _clock_format = ClockFormat::FORMAT_24H;
    bool _is_clock_out_of_area = false;
    ESP_Brookesia_LvObj_t _clock_obj;
    ESP_Brookesia_LvObj_t _clock_hour_label;
    ESP_Brookesia_LvObj_t _clock_dot_label;
    ESP_Brookesia_LvObj_t _clock_min_label;
    ESP_Brookesia_LvObj_t _clock_period_label;
};

} // namespace esp_brookesia::systems::phone

using ESP_Brookesia_StatusBarAreaAlign_t [[deprecated("Use `esp_brookesia::systems::phone::StatusBar::AreaAlign` instead")]] =
    esp_brookesia::systems::phone::StatusBar::AreaAlign;
#define ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_UNKNOWN ESP_Brookesia_StatusBarAreaAlign_t::UNKNOWN
#define ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_START   ESP_Brookesia_StatusBarAreaAlign_t::START
#define ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_END     ESP_Brookesia_StatusBarAreaAlign_t::END
#define ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_CENTER  ESP_Brookesia_StatusBarAreaAlign_t::CENTER
#define ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_MAX     ESP_Brookesia_StatusBarAreaAlign_t::MAX
using ESP_Brookesia_StatusBarAreaData_t [[deprecated("Use `esp_brookesia::systems::phone::StatusBar::AreaData` instead")]] =
    esp_brookesia::systems::phone::StatusBar::AreaData;
using ESP_Brookesia_StatusBarVisualMode_t [[deprecated("Use `esp_brookesia::systems::phone::StatusBar::VisualMode` instead")]] =
    esp_brookesia::systems::phone::StatusBar::VisualMode;
#define ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_HIDE       ESP_Brookesia_StatusBarVisualMode_t::HIDE
#define ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_SHOW_FIXED ESP_Brookesia_StatusBarVisualMode_t::SHOW_FIXED
#define ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_SHOW_FLEX  ESP_Brookesia_StatusBarVisualMode_t::SHOW_FLEX
#define ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_MAX        ESP_Brookesia_StatusBarVisualMode_t::MAX
using ESP_Brookesia_StatusBarData_t [[deprecated("Use `esp_brookesia::systems::phone::StatusBar::Data` instead")]] =
    esp_brookesia::systems::phone::StatusBar::Data;
using ESP_Brookesia_StatusBar [[deprecated("Use `esp_brookesia::systems::phone::StatusBar` instead")]] =
    esp_brookesia::systems::phone::StatusBar;
#define ESP_BROOKESIA_STATUS_BAR_DATA_AREA_NUM_MAX esp_brookesia::systems::phone::StatusBar::AREA_NUM_MAX
