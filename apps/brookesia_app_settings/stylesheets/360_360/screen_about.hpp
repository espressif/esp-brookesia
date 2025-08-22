/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_brookesia.hpp"
#include "esp_brookesia_app_settings_ui.hpp"
#include "assets/esp_brookesia_app_settings_assets.h"

namespace esp_brookesia::apps {

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_SYSTEM_NAME(const char *name)
{
    return {
        .left_main_label_text = name,
        .flags = {
            .enable_left_main_label = 1,
            .enable_right_main_label = 1,
        },
    };
}

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME(const char *name)
{
    return {
        .left_main_label_text = name,
        .flags = {
            .enable_left_main_label = 1,
        },
    };
}

constexpr SettingsUI_ScreenAboutData SETTINGS_UI_360_360_SCREEN_ABOUT_DATA()
{
    return {
        .container_confs = {
            [(int)SettingsUI_ScreenAboutContainerIndex::SYSTEM] = {
                .title_text = "System",
                .flags = {
                    .enable_title = 1,
                },
            },
            [(int)SettingsUI_ScreenAboutContainerIndex::DEVICE] = {
                .title_text = "Device",
                .flags = {
                    .enable_title = 1,
                },
            },
            [(int)SettingsUI_ScreenAboutContainerIndex::CHIP] = {
                .title_text = "Chip",
                .flags = {
                    .enable_title = 1,
                },
            },
        },
        .cell_confs = {
            [(int)SettingsUI_ScreenAboutCellIndex::SYSTEM_FIRMWARE_VERSION] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_SYSTEM_NAME("Firmware"),
            [(int)SettingsUI_ScreenAboutCellIndex::SYSTEM_OS_NAME] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_SYSTEM_NAME("OS"),
            [(int)SettingsUI_ScreenAboutCellIndex::SYSTEM_OS_VERSION] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_SYSTEM_NAME("OS version"),
            [(int)SettingsUI_ScreenAboutCellIndex::SYSTEM_UI_NAME] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_SYSTEM_NAME("UI"),
            [(int)SettingsUI_ScreenAboutCellIndex::SYSTEM_UI_VERSION] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_SYSTEM_NAME("UI version"),
            [(int)SettingsUI_ScreenAboutCellIndex::DEVICE_MANUFACTURER] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME("Manufacturer"),
            [(int)SettingsUI_ScreenAboutCellIndex::DEVICE_NAME] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME("Board"),
            [(int)SettingsUI_ScreenAboutCellIndex::DEVICE_RESOLUTION] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME("Resolution"),
            [(int)SettingsUI_ScreenAboutCellIndex::DEVICE_FLASH_SIZE] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME("Flash"),
            [(int)SettingsUI_ScreenAboutCellIndex::DEVICE_RAM_SIZE] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME("RAM"),
            [(int)SettingsUI_ScreenAboutCellIndex::DEVICE_BATTERY_CAPACITY] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME("Battery capacity"),
            [(int)SettingsUI_ScreenAboutCellIndex::DEVICE_BATTERY_VOLTAGE] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME("Battery voltage"),
            [(int)SettingsUI_ScreenAboutCellIndex::DEVICE_BATTERY_CURRENT] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME("Battery current"),
            [(int)SettingsUI_ScreenAboutCellIndex::CHIP_NAME] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME("Name"),
            [(int)SettingsUI_ScreenAboutCellIndex::CHIP_VERSION] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME("Version"),
            [(int)SettingsUI_ScreenAboutCellIndex::CHIP_MAC] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME("MAC"),
            [(int)SettingsUI_ScreenAboutCellIndex::CHIP_FEATURES] =
            SETTINGS_UI_360_360_SCREEN_ABOUT_ELEMENT_CONF_GENERAL_NAME("Features"),
        },
    };
}

} // namespace esp_brookesia::apps
