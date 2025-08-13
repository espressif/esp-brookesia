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

constexpr SettingsUI_WidgetCellData SETTINGS_UI_360_360_WIDGET_CELL_DATA()
{
    return {
        .main = {
            .size = gui::StyleSize::RECT_W_PERCENT(100, 48),
            .radius = 8,
            .active_background_color = gui::StyleColor::COLOR_WITH_OPACITY(0xFFFFFF, LV_OPA_10),
            .inactive_background_color = gui::StyleColor::COLOR_WITH_OPACITY(0, 0),
        },
        .area = {
            .left_align_x_offset = 20,
            .left_column_pad = 16,
            .right_align_x_offset = 20,
            .right_column_pad = 8,
        },
        .icon = {
            .left_size = gui::StyleSize::SQUARE(36),
            .right_size = gui::StyleSize::SQUARE(24),
        },
        .sw = {
            .main_size = gui::StyleSize::RECT(64, 32),
            .active_indicator_color = gui::StyleColor::COLOR(0xFF3034),
            .inactive_indicator_color = gui::StyleColor::COLOR(0xAAAAAA),
            .knob_size = gui::StyleSize::SQUARE_PERCENT(80),
            .knob_color = gui::StyleColor::COLOR(0xFFFFFF),
        },
        .split_line = {
            .width = 2,
            .color = gui::StyleColor::COLOR_WITH_OPACITY(0xFFFFFF, 64),
        },
        .label = {
            .left_row_pad = 8,
            .right_row_pad = 4,
            .left_main_text_font = gui::StyleFont::SIZE(22),
            .left_main_text_color = gui::StyleColor::COLOR(0xFFFFFF),
            .left_minor_text_font = gui::StyleFont::SIZE(20),
            .left_minor_text_color = gui::StyleColor::COLOR(0xAAAAAA),
            .right_main_text_font = gui::StyleFont::SIZE(20),
            .right_main_text_color = gui::StyleColor::COLOR(0xAAAAAA),
            .right_minor_text_font = gui::StyleFont::SIZE(20),
            .right_minor_text_color = gui::StyleColor::COLOR(0xAAAAAA),
        },
        .text_edit = {
            .size = gui::StyleSize::RECT_PERCENT(80, 100),
            .text_font = gui::StyleFont::SIZE(20),
            .text_color = gui::StyleColor::COLOR(0xFFFFFF),
            .cursor_color = gui::StyleColor::COLOR(0xFFFFFF),
        },
        .slider = {
            .main_size = gui::StyleSize::RECT_W_PERCENT(60, 24),
            .main_color = gui::StyleColor::COLOR(0xAAAAAA),
            .indicator_color = gui::StyleColor::COLOR(0xFF3034),
            .knob_size = gui::StyleSize::SQUARE(0),
            .knob_color = gui::StyleColor::COLOR_WITH_OPACITY(0xFFFFFF, 0),
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
            .text_font = gui::StyleFont::SIZE(20),
            .text_color = gui::StyleColor::COLOR(0xAAAAAA),
        },
        .container = {
            .size = gui::StyleSize::RECT_PERCENT(80, 100),
            .radius = 16,
            .background_color = gui::StyleColor::COLOR(0x38393A),
            .top_pad = 8,
            .bottom_pad = 8,
            .left_pad = 16,
            .right_pad = 16,
        },
        .cell = SETTINGS_UI_360_360_WIDGET_CELL_DATA(),
    };
}

} // namespace esp_brookesia::apps
