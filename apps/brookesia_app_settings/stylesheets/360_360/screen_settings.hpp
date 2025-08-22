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

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_SETTINGS_ELEMENT_CONF_WIRELESS(const auto &icon, const char *left_text, const char *right_text)
{
    return {
        .left_icon_image = gui::StyleImage::IMAGE(&icon),
        .left_main_label_text = left_text,
        .right_main_label_text = right_text,
        .right_icon_images = {
            gui::StyleImage::IMAGE(&esp_brookesia_app_icon_arrow_right_48_48),
        },
        .flags = {
            .enable_left_icon = 1,
            .enable_left_main_label = 1,
            .enable_right_main_label = 1,
            .enable_right_icons = 1,
            .enable_clickable = 1,
        },
    };
}

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_SETTINGS_ELEMENT_CONF_GENERAL(const auto &icon, const char *text)
{
    return {
        .left_icon_image = gui::StyleImage::IMAGE(&icon),
        .left_main_label_text = text,
        .right_icon_images = {
            gui::StyleImage::IMAGE(&esp_brookesia_app_icon_arrow_right_48_48),
        },
        .flags = {
            .enable_left_icon = 1,
            .enable_left_main_label = 1,
            .enable_right_icons = 1,
            .enable_clickable = 1,
        },
    };
}

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_SETTINGS_ELEMENT_CONF_MORE_RESTORE(const auto &icon, const char *text)
{
    return {
        .left_icon_image = gui::StyleImage::IMAGE(&icon),
        .left_main_label_text = text,
        .flags = {
            .enable_left_icon = 1,
            .enable_left_main_label = 1,
            .enable_clickable = 1,
        },
    };
}

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_SETTINGS_ELEMENT_CONF_INPUT_TOUCH(const auto &icon, const char *text)
{
    return {
        .left_icon_image = gui::StyleImage::IMAGE(&icon),
        .left_main_label_text = text,
        .flags = {
            .enable_left_icon = 1,
            .enable_left_main_label = 1,
        },
    };
}

constexpr SettingsUI_ScreenSettingsData SETTINGS_UI_360_360_SCREEN_SETTINGS_DATA()
{
    return {
        .container_confs = {
            [(int)SettingsUI_ScreenSettingsContainerIndex::WIRELESS] = {
                .title_text = "Wireless",
                .flags = {
                    .enable_title = 1,
                },
            },
            [(int)SettingsUI_ScreenSettingsContainerIndex::MEDIA] = {
                .title_text = "Media",
                .flags = {
                    .enable_title = 1,
                },
            },
            [(int)SettingsUI_ScreenSettingsContainerIndex::INPUT] = {
                .title_text = "Input",
                .flags = {
                    .enable_title = 1,
                },
            },
            [(int)SettingsUI_ScreenSettingsContainerIndex::MORE] = {
                .title_text = "More",
                .flags = {
                    .enable_title = 1,
                },
            },
        },
        .cell_confs = {
            [(int)SettingsUI_ScreenSettingsCellIndex::WIRELESS_WLAN] =
            SETTINGS_UI_360_360_SCREEN_SETTINGS_ELEMENT_CONF_WIRELESS(
                esp_brookesia_app_icon_wireless_wlan_48_48, "WLAN", "Off"
            ),
            [(int)SettingsUI_ScreenSettingsCellIndex::MEDIA_SOUND] =
            SETTINGS_UI_360_360_SCREEN_SETTINGS_ELEMENT_CONF_GENERAL(
                esp_brookesia_app_icon_media_sound_48_48, "Sound"
            ),
            [(int)SettingsUI_ScreenSettingsCellIndex::MEDIA_DISPLAY] =
            SETTINGS_UI_360_360_SCREEN_SETTINGS_ELEMENT_CONF_GENERAL(
                esp_brookesia_app_icon_media_display_48_48, "Display"
            ),
            [(int)SettingsUI_ScreenSettingsCellIndex::INPUT_TOUCH] =
            SETTINGS_UI_360_360_SCREEN_SETTINGS_ELEMENT_CONF_INPUT_TOUCH(
                esp_brookesia_app_icon_input_touch_48_48, "Touch"
            ),
            [(int)SettingsUI_ScreenSettingsCellIndex::MORE_ABOUT] =
            SETTINGS_UI_360_360_SCREEN_SETTINGS_ELEMENT_CONF_GENERAL(
                esp_brookesia_app_icon_more_about_48_48, "About"
            ),
            [(int)SettingsUI_ScreenSettingsCellIndex::MORE_DEVELOPER_MODE] =
            SETTINGS_UI_360_360_SCREEN_SETTINGS_ELEMENT_CONF_MORE_RESTORE(
                esp_brookesia_app_icon_more_developer_mode_48_48, "Developer Mode"
            ),
            [(int)SettingsUI_ScreenSettingsCellIndex::MORE_RESTORE] =
            SETTINGS_UI_360_360_SCREEN_SETTINGS_ELEMENT_CONF_MORE_RESTORE(
                esp_brookesia_app_icon_more_restart_48_48, "Restore Factory"
            ),
        },
    };
}

} // namespace esp_brookesia::apps
