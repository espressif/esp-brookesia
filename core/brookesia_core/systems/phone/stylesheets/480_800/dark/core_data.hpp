/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/base/esp_brookesia_base_context.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"

namespace esp_brookesia::systems::phone {

constexpr base::Display::Data STYLESHEET_480_800_DARK_CORE_DISPLAY_DATA = {
    .background = {
        .color = gui::StyleColor::COLOR(0x1A1A1A),
        .wallpaper_image_resource = gui::StyleImage::IMAGE(&esp_brookesia_image_middle_wallpaper_dark_480_480),
    },
    .text = {
        .default_fonts_num = 21,
        .default_fonts = {
            gui::StyleFont::CUSTOM_SIZE(8, &esp_brookesia_font_maison_neue_book_8),
            gui::StyleFont::CUSTOM_SIZE(10, &esp_brookesia_font_maison_neue_book_10),
            gui::StyleFont::CUSTOM_SIZE(12, &esp_brookesia_font_maison_neue_book_12),
            gui::StyleFont::CUSTOM_SIZE(14, &esp_brookesia_font_maison_neue_book_14),
            gui::StyleFont::CUSTOM_SIZE(16, &esp_brookesia_font_maison_neue_book_16),
            gui::StyleFont::CUSTOM_SIZE(18, &esp_brookesia_font_maison_neue_book_18),
            gui::StyleFont::CUSTOM_SIZE(20, &esp_brookesia_font_maison_neue_book_20),
            gui::StyleFont::CUSTOM_SIZE(22, &esp_brookesia_font_maison_neue_book_22),
            gui::StyleFont::CUSTOM_SIZE(24, &esp_brookesia_font_maison_neue_book_24),
            gui::StyleFont::CUSTOM_SIZE(26, &esp_brookesia_font_maison_neue_book_26),
            gui::StyleFont::CUSTOM_SIZE(28, &esp_brookesia_font_maison_neue_book_28),
            gui::StyleFont::CUSTOM_SIZE(30, &esp_brookesia_font_maison_neue_book_30),
            gui::StyleFont::CUSTOM_SIZE(32, &esp_brookesia_font_maison_neue_book_32),
            gui::StyleFont::CUSTOM_SIZE(34, &esp_brookesia_font_maison_neue_book_34),
            gui::StyleFont::CUSTOM_SIZE(36, &esp_brookesia_font_maison_neue_book_36),
            gui::StyleFont::CUSTOM_SIZE(38, &esp_brookesia_font_maison_neue_book_38),
            gui::StyleFont::CUSTOM_SIZE(40, &esp_brookesia_font_maison_neue_book_40),
            gui::StyleFont::CUSTOM_SIZE(42, &esp_brookesia_font_maison_neue_book_42),
            gui::StyleFont::CUSTOM_SIZE(44, &esp_brookesia_font_maison_neue_book_44),
            gui::StyleFont::CUSTOM_SIZE(46, &esp_brookesia_font_maison_neue_book_46),
            gui::StyleFont::CUSTOM_SIZE(48, &esp_brookesia_font_maison_neue_book_48),
        },
    },
    .container = {
        .styles = {
            { .outline_width = 1, .outline_color = gui::StyleColor::COLOR(0xeb3b5a), },
            { .outline_width = 2, .outline_color = gui::StyleColor::COLOR(0xfa8231), },
            { .outline_width = 3, .outline_color = gui::StyleColor::COLOR(0xf7b731), },
            { .outline_width = 2, .outline_color = gui::StyleColor::COLOR(0x20bf6b), },
            { .outline_width = 1, .outline_color = gui::StyleColor::COLOR(0x0fb9b1), },
            { .outline_width = 3, .outline_color = gui::StyleColor::COLOR(0x2d98da), },
        },
    },
};

constexpr base::Manager::Data STYLESHEET_480_800_DARK_CORE_MANAGER_DATA = {
    .app = {
        .max_running_num = 3,
    },
    .flags = {
        .enable_app_save_snapshot = 1,
    },
};

constexpr const char *STYLESHEET_480_800_DARK_CORE_INFO_DATA_NAME = "480x800 Dark";

constexpr base::Context::Data STYLESHEET_480_800_DARK_CORE_DATA = {
    .name = STYLESHEET_480_800_DARK_CORE_INFO_DATA_NAME,
    .screen_size = gui::StyleSize::RECT(480, 800),
    .display = STYLESHEET_480_800_DARK_CORE_DISPLAY_DATA,
    .manager = STYLESHEET_480_800_DARK_CORE_MANAGER_DATA,
};

} // namespace esp_brookesia::systems::phone
