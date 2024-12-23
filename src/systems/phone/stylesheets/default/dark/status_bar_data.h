/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "widgets/status_bar/esp_brookesia_status_bar_type.h"
#include "assets/esp_brookesia_assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Area */
#define ESP_BROOKESIA_PHONE_DEFAULT_DARK_STATUS_BAR_AREA_DATA(w_percent, align) \
    {                                                                    \
        .size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(w_percent, 100),          \
        .layout_column_align = align,                                    \
        .layout_column_start_offset = 10,                                \
        .layout_column_pad = 3,                                          \
    }

/* Status Bar */
#define ESP_BROOKESIA_PHONE_DEFAULT_DARK_STATUS_BAR_DATA()                                                       \
    {                                                                                                     \
        .main = {                                                                                         \
            .size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 10),                                              \
            .size_min = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(100, 24),                                        \
            .size_max = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(100, 50),                                        \
            .background_color = ESP_BROOKESIA_STYLE_COLOR(0x38393A),                                             \
            .text_font = ESP_BROOKESIA_STYLE_FONT_HEIGHT_PERCENT(60),                                            \
            .text_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),                                                   \
        },                                                                                                \
        .area = {                                                                                         \
            .num = 2,                                                                                     \
            .data = {                                                                                     \
                ESP_BROOKESIA_PHONE_DEFAULT_DARK_STATUS_BAR_AREA_DATA(50, ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_START),   \
                ESP_BROOKESIA_PHONE_DEFAULT_DARK_STATUS_BAR_AREA_DATA(50, ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_END),     \
            },                                                                                            \
        },                                                                                                \
        .icon_common_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE_PERCENT(60),                                         \
        .battery = {                                                                                      \
            .area_index = 1,                                                                              \
            .icon_data = {                                                                                \
                .icon = {                                                                                 \
                    .image_num = 5,                                                                       \
                    .images = {                                                                           \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_level1_36_36), \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_level2_36_36), \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_level3_36_36), \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_level4_36_36), \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_charge_36_36),  \
                    },                                                                                    \
                },                                                                                        \
            },                                                                                            \
        },                                                                                                \
        .wifi = {                                                                                         \
            .area_index = 1,                                                                              \
            .icon_data = {                                                                                \
                .icon = {                                                                                 \
                    .image_num = 4,                                                                       \
                    .images = {                                                                           \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_close_36_36),      \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_level1_36_36),    \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_level2_36_36),    \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_level3_36_36),    \
                    },                                                                                    \
                },                                                                                        \
            },                                                                                            \
        },                                                                                                \
        .clock = {                                                                                        \
            .area_index = 0,                                                                              \
        },                                                                                                \
        .flags = {                                                                                        \
            .enable_main_size_min = 1,                                                                    \
            .enable_main_size_max = 1,                                                                    \
            .enable_battery_icon = 1,                                                                     \
            .enable_battery_icon_common_size = 1,                                                         \
            .enable_battery_label = 0,                                                                    \
            .enable_wifi_icon = 1,                                                                        \
            .enable_wifi_icon_common_size = 1,                                                            \
            .enable_clock = 1,                                                                            \
        },                                                                                                \
    }

#ifdef __cplusplus
}
#endif
