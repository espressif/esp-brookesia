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

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_DISPLAY_ELEMENT_CONF_DISPLAY_BRIGHTNESS_SLIDER()
{
    return {
        .left_icon_size = gui::StyleSize::SQUARE(36),
        .left_icon_image = gui::StyleImage::IMAGE(&esp_brookesia_app_icon_brightness_less_48_48),
        .right_icon_size = gui::StyleSize::SQUARE(36),
        .right_icon_images = {
            {
                gui::StyleImage::IMAGE(&esp_brookesia_app_icon_brightness_more_48_48),
            },
        },
        .flags = {
            .enable_left_icon = 1,
            .enable_right_icons = 1,
        },
    };
}

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_DISPLAY_ELEMENT_CONF_DISPLAY_BRIGHTNESS_AUTO()
{
    return {
        .left_main_label_text = "Auto adjust",
        .flags = {
            .enable_left_main_label = 1,
        },
    };
}

constexpr SettingsUI_ScreenDisplayData SETTINGS_UI_360_360_SCREEN_DISPLAY_DATA()
{
    return {
        .container_confs = {
            [(int)SettingsUI_ScreenDisplayContainerIndex::BRIGHTNESS] = {
                .title_text = "Brightness",
                .flags = {
                    .enable_title = 1,
                },
            },
        },
        .cell_confs = {
            [(int)SettingsUI_ScreenDisplayCellIndex::BRIGHTNESS_SLIDER] = SETTINGS_UI_360_360_SCREEN_DISPLAY_ELEMENT_CONF_DISPLAY_BRIGHTNESS_SLIDER(),
            [(int)SettingsUI_ScreenDisplayCellIndex::BRIGHTNESS_AUTO] = SETTINGS_UI_360_360_SCREEN_DISPLAY_ELEMENT_CONF_DISPLAY_BRIGHTNESS_AUTO(),
        },
    };
}

} // namespace esp_brookesia::apps
