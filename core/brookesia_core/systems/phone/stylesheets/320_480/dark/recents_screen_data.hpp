/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/widgets/recents_screen/esp_brookesia_recents_screen.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"

namespace esp_brookesia::systems::phone {

constexpr RecentsScreenSnapshot::Data STYLESHEET_320_480_DARK_RECENTS_SCREEN_SNAPSHOT_DATA = {
    .main_size = gui::StyleSize::RECT(180, 302),
    .title = {
        .main_size = gui::StyleSize::RECT(180, 32),
        .main_layout_column_pad = 5,
        .icon_size = gui::StyleSize::SQUARE(24),
        .text_font = gui::StyleFont::SIZE(18),
        .text_color = gui::StyleColor::COLOR(0xFFFFFF),
    },
    .image = {
        .main_size = gui::StyleSize::RECT(180, 270),
        .radius = 20,
    },
};

constexpr RecentsScreen::Data STYLESHEET_320_480_DARK_RECENTS_SCREEN_DATA = {
    .main = {
        .size = gui::StyleSize::RECT_PERCENT(100, 100),
        .layout_row_pad = 10,
        .layout_top_pad = 0,
        .layout_bottom_pad = 20,
        .background_color = gui::StyleColor::COLOR(0x1A1A1A),
    },
    .memory = {
        .main_size = gui::StyleSize::RECT_W_PERCENT(100, 20),
        .main_layout_x_right_offset = 10,
        .label_text_font = gui::StyleFont::SIZE(16),
        .label_text_color = gui::StyleColor::COLOR(0xFFFFFF),
        .label_unit_text = "KB",
    },
    .snapshot_table = {
        .main_size = gui::StyleSize::RECT_PERCENT(100, 100),
        .main_layout_column_pad = 40,
        .snapshot = STYLESHEET_320_480_DARK_RECENTS_SCREEN_SNAPSHOT_DATA,
    },
    .trash_icon = {
        .default_size = gui::StyleSize::SQUARE(38),
        .press_size = gui::StyleSize::SQUARE(34),
        .image = gui::StyleImage::IMAGE(&esp_brookesia_image_small_recents_screen_trash_38_38),
    },
    .flags = {
        .enable_memory = 1,
        .enable_table_height_flex = 1,
        .enable_table_snapshot_use_icon_image = 0,
    },
};

} // namespace esp_brookesia::systems::phone
