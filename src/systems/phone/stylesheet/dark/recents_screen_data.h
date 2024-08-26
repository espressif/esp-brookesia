/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "widgets/recents_screen/esp_ui_recents_screen_type.h"

#ifdef __cplusplus
extern "C" {
#endif

LV_IMG_DECLARE(esp_ui_phone_recents_screen_image_trash);

#define ESP_UI_PHONE_DEFAULT_DARK_RECENTS_SCREEN_SNAPSHOT_DATA()   \
    {                                                              \
        .main_size = ESP_UI_STYLE_SIZE_RECT_PERCENT(40, 47),       \
        .title = {                                                 \
            .main_size = ESP_UI_STYLE_SIZE_RECT_W_PERCENT(40, 16), \
            .main_layout_column_pad = 10,                          \
            .icon_size = ESP_UI_STYLE_SIZE_SQUARE_PERCENT(100),    \
            .text_font = ESP_UI_STYLE_FONT_HEIGHT_PERCENT(80),     \
            .text_color = ESP_UI_STYLE_COLOR(0xFFFFFF),            \
        },                                                         \
        .image = {                                                 \
            .main_size = ESP_UI_STYLE_SIZE_RECT_PERCENT(40, 40),   \
            .radius = 0,                                           \
        },                                                         \
        .flags = {                                                 \
            .enable_all_main_size_refer_screen = 1,                \
        },                                                         \
    }

#define ESP_UI_PHONE_DEFAULT_DARK_RECENTS_SCREEN_DATA()                           \
    {                                                                             \
        .main = {                                                                 \
            .y_start = 0,                                                         \
            .size = ESP_UI_STYLE_SIZE_RECT_PERCENT(100, 100),                     \
            .layout_row_pad = 10,                                                 \
            .layout_top_pad = 10,                                                 \
            .layout_bottom_pad = 10,                                              \
            .background_color = ESP_UI_STYLE_COLOR(0x1A1A1A),                     \
        },                                                                        \
        .memory = {                                                               \
            .main_size = ESP_UI_STYLE_SIZE_RECT_W_PERCENT(100, 16),               \
            .main_layout_x_right_offset = 10,                                     \
            .label_text_font = ESP_UI_STYLE_FONT_HEIGHT_PERCENT(100),             \
            .label_text_color = ESP_UI_STYLE_COLOR(0xFFFFFF),                     \
            .label_unit_text = "KB",                                              \
        },                                                                        \
        .snapshot_table = {                                                       \
            .main_size = ESP_UI_STYLE_SIZE_RECT_PERCENT(100, 100),                \
            .main_layout_column_pad = 20,                                         \
            .snapshot = ESP_UI_PHONE_DEFAULT_DARK_RECENTS_SCREEN_SNAPSHOT_DATA(), \
        },                                                                        \
        .trash_icon = {                                                           \
            .default_size = ESP_UI_STYLE_SIZE_SQUARE(48),                         \
            .press_size = ESP_UI_STYLE_SIZE_SQUARE(43),                           \
            .image = ESP_UI_STYLE_IMAGE(&esp_ui_phone_recents_screen_image_trash),\
        },                                                                        \
        .flags = {                                                                \
            .enable_memory = 1,                                                   \
            .enable_table_height_flex = 1,                                        \
            .enable_table_snapshot_use_icon_image = 0,                            \
            .enable_table_scroll_anim = 0,                                        \
        },                                                                        \
    }

#ifdef __cplusplus
}
#endif
