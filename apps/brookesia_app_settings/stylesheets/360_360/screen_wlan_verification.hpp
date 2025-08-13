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

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_WLAN_VERIFICATION_ELEMENT_CONF_PASSWORD()
{
    return {
        .left_text_edit_placeholder_text = "Password",
        .flags = {
            .enable_left_main_label = 0,
            .enable_left_text_edit_placeholder = 1,
        },
    };
}

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_WLAN_VERIFICATION_ELEMENT_CONF_ADVANCED_PROXY()
{
    return {
        .left_main_label_text = "Proxy",
        .right_main_label_text = "None",
        .flags = {
            .enable_left_main_label = 1,
            .enable_right_main_label = 1,
        },
    };
}

constexpr SettingsUI_WidgetCellConf SETTINGS_UI_360_360_SCREEN_WLAN_VERIFICATION_ELEMENT_CONF_ADVANCED_IP()
{
    return {
        .left_main_label_text = "IP",
        .right_main_label_text = "DHCP",
        .flags = {
            .enable_left_main_label = 1,
            .enable_right_main_label = 1,
        },
    };
}

constexpr SettingsUI_ScreenWlanVerificationData SETTINGS_UI_360_360_SCREEN_WLAN_VERIFICATION_DATA()
{
    return {
        .container_confs = {
            [(int)SettingsUI_ScreenWlanVerificationContainerIndex::PASSWORD] = {
                .title_text = "",
                .flags = {
                    .enable_title = 0,
                },
            },
            [(int)SettingsUI_ScreenWlanVerificationContainerIndex::ADVANCED] = {
                .title_text = "Advanced settings",
                .flags = {
                    .enable_title = 1,
                },
            },
        },
        .cell_confs = {
            [(int)SettingsUI_ScreenWlanVerificationCellIndex::PASSWORD_EDIT] =
            SETTINGS_UI_360_360_SCREEN_WLAN_VERIFICATION_ELEMENT_CONF_PASSWORD(),
            [(int)SettingsUI_ScreenWlanVerificationCellIndex::ADVANCED_PROXY] =
            SETTINGS_UI_360_360_SCREEN_WLAN_VERIFICATION_ELEMENT_CONF_ADVANCED_PROXY(),
            [(int)SettingsUI_ScreenWlanVerificationCellIndex::ADVANCED_IP] =
            SETTINGS_UI_360_360_SCREEN_WLAN_VERIFICATION_ELEMENT_CONF_ADVANCED_IP(),
        },
        .keyboard = {
            .size = gui::StyleSize::RECT_W_PERCENT(100, 220),
            .align_bottom_offset = 0,
            .top_pad = 0,
            .bottom_pad = 40,
            .left_pad = 20,
            .right_pad = 20,
            .main_background_color = gui::StyleColor::COLOR(0x000000),
            .normal_button_background_color = gui::StyleColor::COLOR(0x000000),
            .special_button_background_color = gui::StyleColor::COLOR(0x000000),
            .ok_button_disabled_background_color = gui::StyleColor::COLOR_WITH_OPACITY(0xFF3034, 80),
            .ok_button_enabled_background_color = gui::StyleColor::COLOR(0xFF3034),
            .text_font = gui::StyleFont::SIZE(24),
            .text_color = gui::StyleColor::COLOR(0xFFFFFF),
        },
    };
}

} // namespace esp_brookesia::apps
