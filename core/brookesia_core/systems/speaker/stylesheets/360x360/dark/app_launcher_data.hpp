/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "style/esp_brookesia_gui_style.hpp"
#include "widgets/app_launcher/esp_brookesia_app_launcher.hpp"

namespace esp_brookesia::systems::speaker {

constexpr AppLauncherIconData STYLESHEET_360_360_DARK_APP_LAUNCHER_ICON_DATA = {
    .main = {
        .size = gui::StyleSize::SQUARE(140),
        .layout_row_pad = 10,
    },
    .image = {
        .default_size = gui::StyleSize::SQUARE(98),
        .press_size = gui::StyleSize::SQUARE(88),
    },
    .label = {
        .text_font = gui::StyleFont::SIZE(16),
        .text_color = gui::StyleColor::COLOR(0xFFFFFF),
    },
};

constexpr AppLauncherData STYLESHEET_360_360_DARK_APP_LAUNCHER_DATA = {
    .main = {
        .y_start = 0,
        .size = gui::StyleSize::RECT_PERCENT(100, 100),
    },
    .table = {
        .default_num = 3,
        .size = gui::StyleSize::RECT_PERCENT(100, 50),
        .align = {
            .type = gui::STYLE_ALIGN_TYPE_CENTER,
            .offset_x = 0,
            .offset_y = 0,
        },
    },
    .indicator = {
        .main_size = gui::StyleSize::RECT_PERCENT(100, 10),
        .main_align = {
            .type = gui::STYLE_ALIGN_TYPE_BOTTOM_MID,
            .offset_x = 0,
            .offset_y = -30,
        },
        .main_gap = gui::StyleGap::COLUMN(10),
        .spot_inactive_size = gui::StyleSize::SQUARE(12),
        .spot_active_size = gui::StyleSize::RECT(40, 12),
        .spot_inactive_background_color = gui::StyleColor::COLOR(0xC6C6C6),
        .spot_active_background_color = gui::StyleColor::COLOR(0xFFFFFF),
    },
    .icon = STYLESHEET_360_360_DARK_APP_LAUNCHER_ICON_DATA,
    .flags = {
        .enable_table_scroll_anim = 0,
    },
};

} // namespace esp_brookesia::systems::speaker
