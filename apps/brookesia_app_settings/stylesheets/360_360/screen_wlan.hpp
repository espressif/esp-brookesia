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

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_WLAN_ELEMENT_CONF_SW(const char *label_text)
{
    return {
        .left_main_label_text = label_text,
        .flags = {
            .enable_left_main_label = 1,
        },
    };
}

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_WLAN_ELEMENT_CONF_CONNECTED_AP()
{
    return {
        .left_main_label_text = "",
        .left_minor_label_text = "",
        .flags = {
            .enable_left_main_label = 1,
            .enable_left_minor_label = 1,
            .enable_clickable = 0,
        },
    };
}

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_WLAN_ELEMENT_CONF_PROVISIONING_SOFTAP()
{
    return {
        .left_main_label_text = "SoftAP Mode",
        .right_icon_images = {
            gui::StyleImage::IMAGE(&esp_brookesia_app_icon_arrow_right_48_48),
        },
        .flags = {
            .enable_left_main_label = 1,
            .enable_right_icons = 1,
            .enable_clickable = 1,
        },
    };
}

constexpr SettingsUI_ScreenWlanData SETTINGS_UI_360_360_SCREEN_WLAN_DATA()
{
    return {
        .container_confs = {
            [(int)SettingsUI_ScreenWlanContainerIndex::CONTROL] = {
                .title_text = "",
                .flags = {
                    .enable_title = 0,
                },
            },
            [(int)SettingsUI_ScreenWlanContainerIndex::CONNECTED] = {
                .title_text = "Connected network",
                .flags = {
                    .enable_title = 1,
                },
            },
            [(int)SettingsUI_ScreenWlanContainerIndex::AVAILABLE] = {
                .title_text = "Available networks",
                .flags = {
                    .enable_title = 1,
                },
            },
            [(int)SettingsUI_ScreenWlanContainerIndex::PROVISIONING] = {
                .title_text = "Provisioning",
                .flags = {
                    .enable_title = 1,
                },
            },
        },
        .cell_confs = {
            [(int)SettingsUI_ScreenWlanCellIndex::CONTROL_SW] =
            SETTINGS_UI_360_360_SCREEN_WLAN_ELEMENT_CONF_SW("WLAN"),
            [(int)SettingsUI_ScreenWlanCellIndex::CONNECTED_AP] =
            SETTINGS_UI_360_360_SCREEN_WLAN_ELEMENT_CONF_CONNECTED_AP(),
            [(int)SettingsUI_ScreenWlanCellIndex::PROVISIONING_SOFTAP] =
            SETTINGS_UI_360_360_SCREEN_WLAN_ELEMENT_CONF_PROVISIONING_SOFTAP(),
        },
        .icon_wlan_signals = {
            gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_app_icon_wlan_level1_36_36),
            gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_app_icon_wlan_level2_36_36),
            gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_app_icon_wlan_level3_36_36),
        },
        .icon_wlan_lock = gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_app_icon_wlan_lock_48_48),
        .cell_left_main_label_size = gui::StyleSize::RECT(200, 24),
    };
}

} // namespace esp_brookesia::apps
