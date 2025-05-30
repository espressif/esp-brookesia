/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/widgets/app_launcher/esp_brookesia_app_launcher.hpp"

constexpr ESP_Brookesia_AppLauncherIconData_t ESP_BROOKESIA_PHONE_480_480_DARK_APP_LAUNCHER_ICON_DATA()
{
    return {
        .main = {
            .size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(180),
            .layout_row_pad = 10,
        },
        .image = {
            .default_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(112),
            .press_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(100),
        },
        .label = {
            .text_font = ESP_BROOKESIA_STYLE_FONT_SIZE(22),
            .text_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),
        }
    };
}

constexpr ESP_Brookesia_AppLauncherData_t ESP_BROOKESIA_PHONE_480_480_DARK_APP_LAUNCHER_DATA()
{
    return {
        .main = {
            .y_start = 0,
            .size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 100),
        },
        .table = {
            .default_num = 3,
            .size = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(100, 360),
        },
        .indicator = {
            .main_size = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(100, 80),
            .main_layout_column_pad = 10,
            .main_layout_bottom_offset = 0,
            .spot_inactive_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(12),
            .spot_active_size = ESP_BROOKESIA_STYLE_SIZE_RECT(30, 12),
            .spot_inactive_background_color = ESP_BROOKESIA_STYLE_COLOR(0xC6C6C6),
            .spot_active_background_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),
        },
        .icon = ESP_BROOKESIA_PHONE_480_480_DARK_APP_LAUNCHER_ICON_DATA(),
        .flags = {
            .enable_table_scroll_anim = 0,
        },
    };
}
