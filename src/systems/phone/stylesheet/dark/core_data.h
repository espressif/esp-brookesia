/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "core/esp_ui_core_type.h"
#include "fonts/esp_ui_fonts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Home */
#define ESP_UI_PHONE_DEFAULT_DARK_CORE_HOME_BG_COLOR    (0x1A1A1A)
#define ESP_UI_PHONE_DEFAULT_DARK_CORE_HOME_DATA()                                      \
    {                                                                                   \
        .background = {                                                                 \
            .color = ESP_UI_STYLE_COLOR(ESP_UI_PHONE_DEFAULT_DARK_CORE_HOME_BG_COLOR),  \
            .wallpaper_image_resource = NULL,                                           \
        },                                                                              \
        .text = {                                                                       \
            .default_fonts_num = 21,                                                    \
            .default_fonts = {                                                          \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(8, &esp_ui_font_maison_neue_book_8),      \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(10, &esp_ui_font_maison_neue_book_10),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(12, &esp_ui_font_maison_neue_book_12),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(14, &esp_ui_font_maison_neue_book_14),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(16, &esp_ui_font_maison_neue_book_16),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(18, &esp_ui_font_maison_neue_book_18),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(20, &esp_ui_font_maison_neue_book_20),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(22, &esp_ui_font_maison_neue_book_22),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(24, &esp_ui_font_maison_neue_book_24),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(26, &esp_ui_font_maison_neue_book_26),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(28, &esp_ui_font_maison_neue_book_28),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(30, &esp_ui_font_maison_neue_book_30),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(32, &esp_ui_font_maison_neue_book_32),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(34, &esp_ui_font_maison_neue_book_34),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(36, &esp_ui_font_maison_neue_book_36),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(38, &esp_ui_font_maison_neue_book_38),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(40, &esp_ui_font_maison_neue_book_40),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(42, &esp_ui_font_maison_neue_book_42),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(44, &esp_ui_font_maison_neue_book_44),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(46, &esp_ui_font_maison_neue_book_46),    \
                ESP_UI_STYLE_FONT_CUSTOM_SIZE(48, &esp_ui_font_maison_neue_book_48),    \
            },                                                                          \
        },                                                                              \
        .container = {                                                                  \
            .styles = {                                                                 \
                { .outline_width = 1, .outline_color = ESP_UI_STYLE_COLOR(0xeb3b5a), }, \
                { .outline_width = 2, .outline_color = ESP_UI_STYLE_COLOR(0xfa8231), }, \
                { .outline_width = 1, .outline_color = ESP_UI_STYLE_COLOR(0xf7b731), }, \
                { .outline_width = 2, .outline_color = ESP_UI_STYLE_COLOR(0x20bf6b), }, \
                { .outline_width = 1, .outline_color = ESP_UI_STYLE_COLOR(0x0fb9b1), }, \
                { .outline_width = 2, .outline_color = ESP_UI_STYLE_COLOR(0x2d98da), }, \
            },                                                                          \
        },                                                                              \
    }

/* manager */
#define ESP_UI_PHONE_DEFAULT_DARK_CORE_MANAGER_DATA() \
    {                                                 \
        .app = {                                      \
            .max_running_num = 3,                     \
        },                                            \
        .flags = {                                    \
            .enable_app_save_snapshot = 1,            \
        },                                            \
    }

/* Core */
#define ESP_UI_PHONE_DEFAULT_DARK_CORE_INFO_DATA_NAME    "EUI-Phone Default Dark"
#define ESP_UI_PHONE_DEFAULT_DARK_CORE_DATA()                     \
    {                                                             \
        .name = ESP_UI_PHONE_DEFAULT_DARK_CORE_INFO_DATA_NAME,    \
        .screen_size = ESP_UI_STYLE_SIZE_RECT_PERCENT(100, 100),  \
        .home = ESP_UI_PHONE_DEFAULT_DARK_CORE_HOME_DATA(),       \
        .manager = ESP_UI_PHONE_DEFAULT_DARK_CORE_MANAGER_DATA(), \
    }

#ifdef __cplusplus
}
#endif
