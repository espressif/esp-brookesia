/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include "esp_brookesia.hpp"
#include "base.hpp"
#include "../widgets/cell_container.hpp"

namespace esp_brookesia::apps {

enum class SettingsUI_ScreenWlanContainerIndex {
    CONTROL,
    CONNECTED,
    AVAILABLE,
    PROVISIONING,
    MAX,
};

enum class SettingsUI_ScreenWlanCellIndex {
    CONTROL_SW,
    CONNECTED_AP,
    PROVISIONING_SOFTAP,
    MAX,
};

struct SettingsUI_ScreenWlanData {
    SettingsUI_WidgetCellContainerConf container_confs[(int)SettingsUI_ScreenWlanContainerIndex::MAX];
    SettingsUI_WidgetCellConf cell_confs[(int)SettingsUI_ScreenWlanCellIndex::MAX];
    gui::StyleImage icon_wlan_signals[3];
    gui::StyleImage icon_wlan_lock;
    gui::StyleColor cell_connected_active_color;
    gui::StyleColor cell_connected_inactive_color;
    gui::StyleSize cell_left_main_label_size;
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
        systems::speaker::App &ui_app, const SettingsUI_ScreenBaseData &base_data,
        const SettingsUI_ScreenWlanData &main_data
    );
    ~SettingsUI_ScreenWlan();

    bool begin();
    bool del();
    bool processDataUpdate();

    // Connected list
    bool setConnectedVisible(bool visible);
    bool updateConnectedData(WlanData wlan_data);
    bool updateConnectedState(ConnectState state);
    bool scrollConnectedToView();
    bool checkConnectedVisible();
    ConnectState getConnectedState() const
    {
        return _connected_state;
    }

    // Available list
    bool setAvailableVisible(bool visible);
    bool updateAvailableData(
        std::vector<WlanData> &&wlan_data, systems::base::Event::Handler event_handler, void *user_data
    );
    bool cleanAvailable();
    bool setAvaliableClickable(bool clickable);

    // SoftAP
    bool setSoftAPVisible(bool visible);

    const SettingsUI_ScreenWlanData &data;

private:
    SettingsUI_ScreenWlanCellContainerMap _cell_container_map;
    ConnectState _connected_state = ConnectState::DISCONNECT;

    bool processCellContainerMapInit();
    bool processCellContainerMapUpdate();
    bool updateCellWlanData(SettingsUI_WidgetCell *cell, WlanData wlan_data);
};

} // namespace esp_brookesia::apps
