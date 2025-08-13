/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "widget_cell_container.hpp"
#include "screen_base.hpp"
#include "screen_settings.hpp"
#include "screen_wlan.hpp"
#include "screen_wlan_verification.hpp"
#include "screen_wlan_softap.hpp"
#include "screen_about.hpp"
#include "screen_sound.hpp"
#include "screen_display.hpp"
#include "esp_brookesia_app_settings_data.hpp"

namespace esp_brookesia::apps {

constexpr SettingsUI_Data SETTINGS_UI_360_360_DATA()
{
    return {
        .screen_base = SETTINGS_UI_360_360_SCREEN_BASE_DATA,
        .screen_settings = SETTINGS_UI_360_360_SCREEN_SETTINGS_DATA(),
        .screen_wlan = SETTINGS_UI_360_360_SCREEN_WLAN_DATA(),
        .screen_wlan_verification = SETTINGS_UI_360_360_SCREEN_WLAN_VERIFICATION_DATA(),
        .screen_wlan_softap = SETTINGS_UI_360_360_SCREEN_WLAN_SOFTAP_DATA(),
        .screen_about = SETTINGS_UI_360_360_SCREEN_ABOUT_DATA(),
        .screen_sound = SETTINGS_UI_360_360_SCREEN_SOUND_DATA(),
        .screen_display = SETTINGS_UI_360_360_SCREEN_DISPLAY_DATA(),
    };
}

constexpr SettingsStylesheetData SETTINGS_UI_360_360_STYLESHEET_DARK()
{
    return {
        .name = "Brookesia-Dark",
        .screen_size = gui::StyleSize::RECT(360, 360),
        .ui = SETTINGS_UI_360_360_DATA(),
    };
}

} // namespace esp_brookesia::apps
