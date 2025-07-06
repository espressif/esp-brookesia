/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_brookesia.hpp"
#include "esp_brookesia_app_settings_ui.hpp"
#include "assets/esp_brookesia_app_settings_assets.h"

namespace esp_brookesia::speaker_apps {

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_WLAN_ELEMENT_CONF_SW()
{
    return {
        .left_main_label_text = "WLAN",
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
        },
        .cell_confs = {
            [(int)SettingsUI_ScreenWlanCellIndex::CONTROL_SW] = SETTINGS_UI_360_360_SCREEN_WLAN_ELEMENT_CONF_SW(),
            [(int)SettingsUI_ScreenWlanCellIndex::CONNECTED_AP] = SETTINGS_UI_360_360_SCREEN_WLAN_ELEMENT_CONF_CONNECTED_AP(),
        },
        .icon_wlan_signals = {
            ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_level1_36_36),
            ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_level2_36_36),
            ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_level3_36_36),
        },
        .icon_wlan_lock = ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_app_icon_wlan_lock_48_48),
    };
}

} // namespace esp_brookesia::speaker_apps
