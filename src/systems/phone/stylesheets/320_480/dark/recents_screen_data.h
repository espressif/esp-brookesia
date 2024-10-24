/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "widgets/recents_screen/esp_brookesia_recents_screen_type.h"
#include "assets/esp_brookesia_assets.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_BROOKESIA_PHONE_320_480_DARK_RECENTS_SCREEN_SNAPSHOT_DATA() \
    {                                                            \
        .main_size = ESP_BROOKESIA_STYLE_SIZE_RECT(180, 302),           \
        .title = {                                               \
            .main_size = ESP_BROOKESIA_STYLE_SIZE_RECT(180, 32),        \
            .main_layout_column_pad = 5,                        \
            .icon_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(24),           \
            .text_font = ESP_BROOKESIA_STYLE_FONT_SIZE(18),             \
            .text_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),          \
        },                                                       \
        .image = {                                               \
            .main_size = ESP_BROOKESIA_STYLE_SIZE_RECT(180, 270),       \
            .radius = 20,                                        \
        },                                                       \
    }

#define ESP_BROOKESIA_PHONE_320_480_DARK_RECENTS_SCREEN_DATA()                                    \
    {                                                                                      \
        .main = {                                                                          \
            .size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 100),                              \
            .layout_row_pad = 10,                                                          \
            .layout_top_pad = 0,                                                           \
            .layout_bottom_pad = 20,                                                       \
            .background_color = ESP_BROOKESIA_STYLE_COLOR(0x1A1A1A),                              \
        },                                                                                 \
        .memory = {                                                                        \
            .main_size = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(100, 20),                        \
            .main_layout_x_right_offset = 10,                                              \
            .label_text_font = ESP_BROOKESIA_STYLE_FONT_SIZE(16),                                 \
            .label_text_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),                              \
            .label_unit_text = "KB",                                                       \
        },                                                                                 \
        .snapshot_table = {                                                                \
            .main_size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 100),                         \
            .main_layout_column_pad = 40,                                                  \
            .snapshot = ESP_BROOKESIA_PHONE_320_480_DARK_RECENTS_SCREEN_SNAPSHOT_DATA(),          \
        },                                                                                 \
        .trash_icon = {                                                                    \
            .default_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(38),                                  \
            .press_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(34),                                    \
            .image = ESP_BROOKESIA_STYLE_IMAGE(&esp_brookesia_image_small_recents_screen_trash_38_38), \
        },                                                                                 \
        .flags = {                                                                         \
            .enable_memory = 1,                                                            \
            .enable_table_height_flex = 1,                                                 \
            .enable_table_snapshot_use_icon_image = 0,                                     \
        },                                                                                 \
    }

#ifdef __cplusplus
}
#endif
