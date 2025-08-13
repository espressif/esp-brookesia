/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "style/esp_brookesia_gui_style.hpp"
#include "esp_brookesia.hpp"
#include "esp_brookesia_app_settings_ui.hpp"
#include "assets/esp_brookesia_app_settings_assets.h"
#include "widget_cell_container.hpp"

namespace esp_brookesia::apps {

// Header
constexpr SettingsUI_ScreenBaseHeaderData SETTINGS_UI_360_360_SCREEN_BASE_HEADER_DATA()
{
    return {
        .size = gui::StyleSize::RECT_W_PERCENT(100, 48),
        .align_top_offset = 10,
        .background_color = gui::StyleColor::COLOR_WITH_OPACITY(0, 0),
        .title_text_font = gui::StyleFont::SIZE(30),
        .title_text_color = gui::StyleColor::COLOR(0xFFFFFF),
    };
}

// Header Navigation
constexpr SettingsUI_ScreenBaseHeaderNavigation SETTINGS_UI_360_360_SCREEN_BASE_HEADER_NAVIGATION_DATA = {
    .align = {
        .type = gui::STYLE_ALIGN_TYPE_CENTER,
        .offset_x = 0,
        .offset_y = 0,
    },
    .main_column_pad = 0,
    .icon_size = gui::StyleSize::SQUARE(24),
    .icon_image = gui::StyleImage::IMAGE_RECOLOR(&esp_brookesia_app_icon_arrow_left_48_48, 0xFF3034),
    .title_text_font = gui::StyleFont::SIZE(24),
    .title_text_color = gui::StyleColor::COLOR(0xFF3034),
};

// Content
constexpr SettingsUI_ScreenBaseContentData SETTINGS_UI_360_360_SCREEN_BASE_CONTENT_DATA()
{
    return {
        .size = gui::StyleSize::RECT_PERCENT(100, 100),
        .align_bottom_offset = 0,
        .background_color = gui::StyleColor::COLOR_WITH_OPACITY(0, 0),
        .row_pad = 16,
        .top_pad = 16,
        .bottom_pad = 76,
        .left_pad = 0,
        .right_pad = 0,
    };
}

// Cell Container
constexpr auto SETTINGS_UI_360_360_SCREEN_BASE_CELL_CONTAINER()
{
    return SETTINGS_UI_360_360_WIDGET_CELL_CONTAINER_DATA();
}

constexpr SettingsUI_ScreenBaseData SETTINGS_UI_360_360_SCREEN_BASE_DATA = {
    .screen = {
        .size = gui::StyleSize::CIRCLE_PERCENT(100),
        .background_color = gui::StyleColor::COLOR(0x1A1A1A),
    },
    .header = SETTINGS_UI_360_360_SCREEN_BASE_HEADER_DATA(),
    .header_navigation = SETTINGS_UI_360_360_SCREEN_BASE_HEADER_NAVIGATION_DATA,
    .content = SETTINGS_UI_360_360_SCREEN_BASE_CONTENT_DATA(),
    .cell_container = SETTINGS_UI_360_360_SCREEN_BASE_CELL_CONTAINER(),
    .flags = {
        .enable_header_title = 0,
        .enable_content_size_flex = 1,
    },
};

} // namespace esp_brookesia::apps
