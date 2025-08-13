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

namespace esp_brookesia::apps {

enum class SettingsUI_ScreenSettingsContainerIndex {
    WIRELESS,
    MEDIA,
    INPUT,
    MORE,
    MAX,
};

enum class SettingsUI_ScreenSettingsCellIndex {
    WIRELESS_WLAN,
    MEDIA_SOUND,
    MEDIA_DISPLAY,
    INPUT_TOUCH,
    MORE_ABOUT,
    MORE_DEVELOPER_MODE,
    MORE_RESTORE,
    MAX,
};

struct SettingsUI_ScreenSettingsData {
    SettingsUI_WidgetCellContainerConf container_confs[(int)SettingsUI_ScreenSettingsContainerIndex::MAX];
    SettingsUI_WidgetCellConf cell_confs[(int)SettingsUI_ScreenSettingsCellIndex::MAX];
};

using SettingsUI_ScreenSettingsCellContainerMap =
    SettingsUI_ScreenBaseCellContainerMap<SettingsUI_ScreenSettingsContainerIndex, SettingsUI_ScreenSettingsCellIndex>;

class SettingsUI_ScreenSettings: public SettingsUI_ScreenBase {
public:
    SettingsUI_ScreenSettings(systems::speaker::App &ui_app, const SettingsUI_ScreenBaseData &base_data,
                              const SettingsUI_ScreenSettingsData &main_data);
    ~SettingsUI_ScreenSettings();

    bool begin();
    bool del();
    bool processDataUpdate();

    const SettingsUI_ScreenSettingsData &data;

private:
    SettingsUI_ScreenSettingsCellContainerMap _cell_container_map;

    bool processCellContainerMapInit();
    bool processCellContainerMapUpdate();
};

} // namespace esp_brookesia::apps
