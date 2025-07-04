/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/core/esp_brookesia_core.hpp"
#include "assets/esp_brookesia_speaker_assets.h"

namespace esp_brookesia::speaker {

/* Display */
constexpr int ESP_BROOKESIA_SPEAKER_360_360_DARK_CORE_DISPLAY_BG_COLOR = 0x1A1A1A;
constexpr ESP_Brookesia_CoreDisplayData ESP_BROOKESIA_SPEAKER_360_360_DARK_CORE_DISPLAY_DATA = {
    .background = {
        .color = ESP_BROOKESIA_STYLE_COLOR(ESP_BROOKESIA_SPEAKER_360_360_DARK_CORE_DISPLAY_BG_COLOR),
        .wallpaper_image_resource = NULL,
    },
    .text = {
        .default_fonts_num = 21,
        .default_fonts = {
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(8, &esp_brookesia_font_maison_neue_book_8),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(10, &esp_brookesia_font_maison_neue_book_10),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(12, &esp_brookesia_font_maison_neue_book_12),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(14, &esp_brookesia_font_maison_neue_book_14),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(16, &esp_brookesia_font_maison_neue_book_16),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(18, &esp_brookesia_font_maison_neue_book_18),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(20, &esp_brookesia_font_maison_neue_book_20),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(22, &esp_brookesia_font_maison_neue_book_22),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(24, &esp_brookesia_font_maison_neue_book_24),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(26, &esp_brookesia_font_maison_neue_book_26),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(28, &esp_brookesia_font_maison_neue_book_28),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(30, &esp_brookesia_font_maison_neue_book_30),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(32, &esp_brookesia_font_maison_neue_book_32),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(34, &esp_brookesia_font_maison_neue_book_34),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(36, &esp_brookesia_font_maison_neue_book_36),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(38, &esp_brookesia_font_maison_neue_book_38),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(40, &esp_brookesia_font_maison_neue_book_40),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(42, &esp_brookesia_font_maison_neue_book_42),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(44, &esp_brookesia_font_maison_neue_book_44),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(46, &esp_brookesia_font_maison_neue_book_46),
            ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(48, &esp_brookesia_font_maison_neue_book_48),
        },
    },
    .container = {
        .styles = {
            { .outline_width = 1, .outline_color = ESP_BROOKESIA_STYLE_COLOR(0xeb3b5a), },
            { .outline_width = 2, .outline_color = ESP_BROOKESIA_STYLE_COLOR(0xfa8231), },
            { .outline_width = 1, .outline_color = ESP_BROOKESIA_STYLE_COLOR(0xf7b731), },
            { .outline_width = 2, .outline_color = ESP_BROOKESIA_STYLE_COLOR(0x20bf6b), },
            { .outline_width = 1, .outline_color = ESP_BROOKESIA_STYLE_COLOR(0x0fb9b1), },
            { .outline_width = 2, .outline_color = ESP_BROOKESIA_STYLE_COLOR(0x2d98da), },
        },
    },
};

/* manager */
constexpr ESP_Brookesia_CoreManagerData_t ESP_BROOKESIA_SPEAKER_360_360_DARK_CORE_MANAGER_DATA = {
    .app = {
        .max_running_num = 1,
    },
    .flags = {
        .enable_app_save_snapshot = 0,
    },
};

/* Core */
constexpr const char *ESP_BROOKESIA_SPEAKER_360_360_DARK_CORE_INFO_DATA_NAME = "360x360 Dark";
constexpr ESP_Brookesia_CoreData_t ESP_BROOKESIA_SPEAKER_360_360_DARK_CORE_DATA = {
    .name = ESP_BROOKESIA_SPEAKER_360_360_DARK_CORE_INFO_DATA_NAME,
    .screen_size = ESP_BROOKESIA_STYLE_SIZE_RECT(360, 360),
    .display = ESP_BROOKESIA_SPEAKER_360_360_DARK_CORE_DISPLAY_DATA,
    .manager = ESP_BROOKESIA_SPEAKER_360_360_DARK_CORE_MANAGER_DATA,
};

} // namespace esp_brookesia::speaker
