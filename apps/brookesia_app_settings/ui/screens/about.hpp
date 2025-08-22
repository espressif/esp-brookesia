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

enum class SettingsUI_ScreenAboutContainerIndex {
    SYSTEM,
    DEVICE,
    CHIP,
    MAX,
};

enum class SettingsUI_ScreenAboutCellIndex {
    SYSTEM_FIRMWARE_VERSION,
    SYSTEM_OS_NAME,
    SYSTEM_OS_VERSION,
    SYSTEM_UI_NAME,
    SYSTEM_UI_VERSION,
    DEVICE_MANUFACTURER,
    DEVICE_NAME,
    DEVICE_RESOLUTION,
    DEVICE_FLASH_SIZE,
    DEVICE_RAM_SIZE,
    DEVICE_BATTERY_CAPACITY,
    DEVICE_BATTERY_VOLTAGE,
    DEVICE_BATTERY_CURRENT,
    CHIP_NAME,
    CHIP_VERSION,
    CHIP_MAC,
    CHIP_FEATURES,
    MAX,
};

struct SettingsUI_ScreenAboutData {
    SettingsUI_WidgetCellContainerConf container_confs[(int)SettingsUI_ScreenAboutContainerIndex::MAX];
    SettingsUI_WidgetCellConf cell_confs[(int)SettingsUI_ScreenAboutCellIndex::MAX];
};

using SettingsUI_ScreenAboutCellContainerMap =
    SettingsUI_ScreenBaseCellContainerMap<SettingsUI_ScreenAboutContainerIndex, SettingsUI_ScreenAboutCellIndex>;

class SettingsUI_ScreenAbout: public SettingsUI_ScreenBase {
public:
    SettingsUI_ScreenAbout(systems::speaker::App &ui_app, const SettingsUI_ScreenBaseData &base_data,
                           const SettingsUI_ScreenAboutData &main_data);
    ~SettingsUI_ScreenAbout();

    bool begin();
    bool del();
    bool processDataUpdate();

    const SettingsUI_ScreenAboutData &data;

private:
    bool processCellContainerMapInit();
    bool processCellContainerMapUpdate();

    SettingsUI_ScreenAboutCellContainerMap _cell_container_map;
};

} // namespace esp_brookesia::apps
