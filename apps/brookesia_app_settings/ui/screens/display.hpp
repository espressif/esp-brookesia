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

enum class SettingsUI_ScreenDisplayContainerIndex {
    BRIGHTNESS,
    MAX,
};

enum class SettingsUI_ScreenDisplayCellIndex {
    BRIGHTNESS_SLIDER,
    BRIGHTNESS_AUTO,
    MAX,
};

struct SettingsUI_ScreenDisplayData {
    SettingsUI_WidgetCellContainerConf container_confs[(int)SettingsUI_ScreenDisplayContainerIndex::MAX];
    SettingsUI_WidgetCellConf cell_confs[(int)SettingsUI_ScreenDisplayCellIndex::MAX];
};

using SettingsUI_ScreenDisplayCellContainerMap =
    SettingsUI_ScreenBaseCellContainerMap<SettingsUI_ScreenDisplayContainerIndex, SettingsUI_ScreenDisplayCellIndex>;

class SettingsUI_ScreenDisplay: public SettingsUI_ScreenBase {
public:
    SettingsUI_ScreenDisplay(systems::speaker::App &ui_app, const SettingsUI_ScreenBaseData &base_data,
                             const SettingsUI_ScreenDisplayData &main_data);
    ~SettingsUI_ScreenDisplay();

    bool begin();
    bool del();
    bool processDataUpdate();

    const SettingsUI_ScreenDisplayData &data;

private:
    bool processCellContainerMapInit();
    bool processCellContainerMapUpdate();

    SettingsUI_ScreenDisplayCellContainerMap _cell_container_map;
};

} // namespace esp_brookesia::apps
