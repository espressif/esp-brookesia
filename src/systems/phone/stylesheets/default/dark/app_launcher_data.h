/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "widgets/app_launcher/esp_brookesia_app_launcher_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_BROOKESIA_PHONE_DEFAULT_DARK_APP_LAUNCHER_ICON_DATA()        \
    {                                                             \
        .main = {                                                 \
            .size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(140),                \
            .layout_row_pad = 10,                                 \
        },                                                        \
        .image = {                                                \
            .default_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(98), \
            .press_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(88),   \
        },                                                        \
        .label = {                                                \
            .text_font = ESP_BROOKESIA_STYLE_FONT_SIZE(16),              \
            .text_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),           \
        },                                                        \
    }

#define ESP_BROOKESIA_PHONE_DEFAULT_DARK_APP_LAUNCHER_DATA()                       \
    {                                                                       \
        .main = {                                                           \
            .y_start = 0,                                                   \
            .size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 100),               \
        },                                                                  \
        .table = {                                                          \
            .default_num = 3,                                               \
            .size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 70),         \
        },                                                                  \
        .indicator = {                                                      \
            .main_size = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(100, 20),  \
            .main_layout_column_pad = 10,                                   \
            .main_layout_bottom_offset = 30,                                 \
            .spot_inactive_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(12),     \
            .spot_active_size = ESP_BROOKESIA_STYLE_SIZE_RECT(40, 12),   \
            .spot_inactive_background_color = ESP_BROOKESIA_STYLE_COLOR(0xC6C6C6), \
            .spot_active_background_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),   \
        },                                                                  \
        .icon = ESP_BROOKESIA_PHONE_DEFAULT_DARK_APP_LAUNCHER_ICON_DATA(),         \
        .flags = {                                                          \
            .enable_table_scroll_anim = 0,                                  \
        },                                                                  \
    }

#ifdef __cplusplus
}
#endif
