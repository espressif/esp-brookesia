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

enum class SettingsUI_ScreenSoundContainerIndex {
    VOLUME,
    MAX,
};

enum class SettingsUI_ScreenSoundCellIndex {
    VOLUME_SLIDER,
    MAX,
};

struct SettingsUI_ScreenSoundData {
    SettingsUI_WidgetCellContainerConf container_confs[(int)SettingsUI_ScreenSoundContainerIndex::MAX];
    SettingsUI_WidgetCellConf cell_confs[(int)SettingsUI_ScreenSoundCellIndex::MAX];
};

using SettingsUI_ScreenSoundCellContainerMap =
    SettingsUI_ScreenBaseCellContainerMap<SettingsUI_ScreenSoundContainerIndex, SettingsUI_ScreenSoundCellIndex>;

class SettingsUI_ScreenSound: public SettingsUI_ScreenBase {
public:
    SettingsUI_ScreenSound(systems::speaker::App &ui_app, const SettingsUI_ScreenBaseData &base_data,
                           const SettingsUI_ScreenSoundData &main_data);
    ~SettingsUI_ScreenSound();

    bool begin();
    bool del();
    bool processDataUpdate();

    const SettingsUI_ScreenSoundData &data;

private:
    bool processCellContainerMapInit();
    bool processCellContainerMapUpdate();

    SettingsUI_ScreenSoundCellContainerMap _cell_container_map;
};

} // namespace esp_brookesia::apps
