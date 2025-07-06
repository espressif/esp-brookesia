/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include "esp_brookesia.hpp"
#include "base.hpp"
#include "../widgets/cell_container.hpp"

namespace esp_brookesia::speaker_apps {

enum class SettingsUI_ScreenWlanContainerIndex {
    CONTROL,
    CONNECTED,
    AVAILABLE,
    MAX,
};

enum class SettingsUI_ScreenWlanCellIndex {
    CONTROL_SW,
    CONNECTED_AP,
    MAX,
};

struct SettingsUI_ScreenWlanData {
    SettingsUI_WidgetCellContainerConf container_confs[(int)SettingsUI_ScreenWlanContainerIndex::MAX];
    SettingsUI_WidgetCellConf cell_confs[(int)SettingsUI_ScreenWlanCellIndex::MAX];
    ESP_Brookesia_StyleImage_t icon_wlan_signals[3];
    ESP_Brookesia_StyleImage_t icon_wlan_lock;
    ESP_Brookesia_StyleColor_t cell_connected_active_color;
    ESP_Brookesia_StyleColor_t cell_connected_inactive_color;
};

using SettingsUI_ScreenWlanCellContainerMap =
    SettingsUI_ScreenBaseCellContainerMap<SettingsUI_ScreenWlanContainerIndex, SettingsUI_ScreenWlanCellIndex>;

class SettingsUI_ScreenWlan: public SettingsUI_ScreenBase {
public:
    typedef enum {
        WEAK = 1,
        MODERATE = 2,
        GOOD = 3,
    } SignalLevel;
    struct WlanData {
        std::string ssid;
        bool is_locked;
        SignalLevel signal_level;
    };
    enum ConnectState {
        CONNECTED,
        CONNECTING,
        DISCONNECT,
    };

    SettingsUI_ScreenWlan(
        speaker::App &ui_app, const SettingsUI_ScreenBaseData &base_data,
        const SettingsUI_ScreenWlanData &main_data
    );
    ~SettingsUI_ScreenWlan();

    bool begin();
    bool del();
    bool processDataUpdate();

    // Connected list
    bool setConnectedVisible(bool visible);
    bool updateConnectedData(const WlanData &wlan_data);
    bool updateConnectedState(ConnectState state);
    bool scrollConnectedToView();
    bool checkConnectedVisible();

    // Available list
    bool setAvailableVisible(bool visible);
    bool updateAvailableData(
        const std::vector<WlanData> &wlan_data, ESP_Brookesia_CoreEvent::Handler event_handler, void *user_data
    );
    bool cleanAvailable();
    bool setAvaliableClickable(bool clickable);

    const SettingsUI_ScreenWlanData &data;

private:
    SettingsUI_ScreenWlanCellContainerMap _cell_container_map;
    // static const SettingsUI_ScreenWlanCellContainerMap _init_cell_container_map;

    bool processCellContainerMapInit();
    bool processCellContainerMapUpdate();
    bool updateCellWlanData(SettingsUI_WidgetCell *cell, const WlanData &wlan_data);
};

} // namespace esp_brookesia::speaker
