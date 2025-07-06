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

constexpr SettingsUI_WidgetCellData SETTINGS_UI_360_360_WIDGET_CELL_DATA()
{
    return {
        .main = {
            .size = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(100, 48),
            .radius = 8,
            .active_background_color = ESP_BROOKESIA_STYLE_COLOR_WITH_OPACITY(0xFFFFFF, LV_OPA_10),
            .inactive_background_color = ESP_BROOKESIA_STYLE_COLOR_WITH_OPACITY(0, 0),
        },
        .area = {
            .left_align_x_offset = 20,
            .left_column_pad = 16,
            .right_align_x_offset = 20,
            .right_column_pad = 8,
        },
        .icon = {
            .left_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(36),
            .right_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(24),
        },
        .sw = {
            .main_size = ESP_BROOKESIA_STYLE_SIZE_RECT(64, 32),
            .active_indicator_color = ESP_BROOKESIA_STYLE_COLOR(0xFF3034),
            .inactive_indicator_color = ESP_BROOKESIA_STYLE_COLOR(0xAAAAAA),
            .knob_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE_PERCENT(80),
            .knob_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),
        },
        .split_line = {
            .width = 2,
            .color = ESP_BROOKESIA_STYLE_COLOR_WITH_OPACITY(0xFFFFFF, 64),
        },
        .label = {
            .left_row_pad = 8,
            .right_row_pad = 4,
            .left_main_text_font = ESP_BROOKESIA_STYLE_FONT_SIZE(22),
            .left_main_text_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),
            .left_minor_text_font = ESP_BROOKESIA_STYLE_FONT_SIZE(20),
            .left_minor_text_color = ESP_BROOKESIA_STYLE_COLOR(0xAAAAAA),
            .right_main_text_font = ESP_BROOKESIA_STYLE_FONT_SIZE(20),
            .right_main_text_color = ESP_BROOKESIA_STYLE_COLOR(0xAAAAAA),
            .right_minor_text_font = ESP_BROOKESIA_STYLE_FONT_SIZE(20),
            .right_minor_text_color = ESP_BROOKESIA_STYLE_COLOR(0xAAAAAA),
        },
        .text_edit = {
            .size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(80, 100),
            .text_font = ESP_BROOKESIA_STYLE_FONT_SIZE(20),
            .text_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),
            .cursor_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),
        },
        .slider = {
            .main_size = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(60, 24),
            .main_color = ESP_BROOKESIA_STYLE_COLOR(0xAAAAAA),
            .indicator_color = ESP_BROOKESIA_STYLE_COLOR(0xFF3034),
            .knob_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(0),
            .knob_color = ESP_BROOKESIA_STYLE_COLOR_WITH_OPACITY(0xFFFFFF, 0),
        },
    };
}

constexpr SettingsUI_WidgetCellContainerData SETTINGS_UI_360_360_WIDGET_CELL_CONTAINER_DATA()
{
    return {
        .main = {
            .row_pad = 8,
        },
        .title = {
            .text_font = ESP_BROOKESIA_STYLE_FONT_SIZE(20),
            .text_color = ESP_BROOKESIA_STYLE_COLOR(0xAAAAAA),
        },
        .container = {
            .size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(80, 100),
            .radius = 16,
            .background_color = ESP_BROOKESIA_STYLE_COLOR(0x38393A),
            .top_pad = 8,
            .bottom_pad = 8,
            .left_pad = 16,
            .right_pad = 16,
        },
        .cell = SETTINGS_UI_360_360_WIDGET_CELL_DATA(),
    };
}

} // namespace esp_brookesia::speaker_apps
