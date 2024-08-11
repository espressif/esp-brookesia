/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "widgets/status_bar/esp_ui_status_bar_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Icon Images */
// Battery
LV_IMG_DECLARE(esp_ui_phone_status_bar_image_battery_charge);
LV_IMG_DECLARE(esp_ui_phone_status_bar_image_battery_level_1);
LV_IMG_DECLARE(esp_ui_phone_status_bar_image_battery_level_2);
LV_IMG_DECLARE(esp_ui_phone_status_bar_image_battery_level_3);
LV_IMG_DECLARE(esp_ui_phone_status_bar_image_battery_level_4);
// WiFi
LV_IMG_DECLARE(esp_ui_phone_status_bar_image_wifi_close);
LV_IMG_DECLARE(esp_ui_phone_status_bar_image_wifi_level_1);
LV_IMG_DECLARE(esp_ui_phone_status_bar_image_wifi_level_2);
LV_IMG_DECLARE(esp_ui_phone_status_bar_image_wifi_level_3);

/* Area */
#define ESP_UI_PHONE_DEFAULT_DARK_STATUS_BAR_AREA_DATA(w_percent, align) \
    {                                                                    \
        .size = ESP_UI_STYLE_SIZE_RECT_PERCENT(w_percent, 100),          \
        .layout_column_align = align,                                    \
        .layout_column_start_offset = 10,                                \
        .layout_column_pad = 3,                                          \
    }

/* Status Bar */
#define ESP_UI_PHONE_DEFAULT_DARK_STATUS_BAR_DATA()                                                       \
    {                                                                                                     \
        .main = {                                                                                         \
            .size = ESP_UI_STYLE_SIZE_RECT_PERCENT(100, 10),                                              \
            .size_min = ESP_UI_STYLE_SIZE_RECT_W_PERCENT(100, 24),                                        \
            .size_max = ESP_UI_STYLE_SIZE_RECT_W_PERCENT(100, 50),                                        \
            .background_color = ESP_UI_STYLE_COLOR(0x38393A),                                             \
            .text_font = ESP_UI_STYLE_FONT_HEIGHT_PERCENT(60),                                            \
            .text_color = ESP_UI_STYLE_COLOR(0xFFFFFF),                                                   \
        },                                                                                                \
        .area = {                                                                                         \
            .num = 2,                                                                                     \
            .data = {                                                                                     \
                ESP_UI_PHONE_DEFAULT_DARK_STATUS_BAR_AREA_DATA(50, ESP_UI_STATUS_BAR_AREA_ALIGN_START),   \
                ESP_UI_PHONE_DEFAULT_DARK_STATUS_BAR_AREA_DATA(50, ESP_UI_STATUS_BAR_AREA_ALIGN_END),     \
            },                                                                                            \
        },                                                                                                \
        .icon_common_size = ESP_UI_STYLE_SIZE_SQUARE_PERCENT(60),                                         \
        .battery = {                                                                                      \
            .area_index = 1,                                                                              \
            .icon_data = {                                                                                \
                .icon = {                                                                                 \
                    .image_num = 5,                                                                       \
                    .images = {                                                                           \
                        ESP_UI_STYLE_IMAGE_RECOLOR_WHITE(&esp_ui_phone_status_bar_image_battery_level_1), \
                        ESP_UI_STYLE_IMAGE_RECOLOR_WHITE(&esp_ui_phone_status_bar_image_battery_level_2), \
                        ESP_UI_STYLE_IMAGE_RECOLOR_WHITE(&esp_ui_phone_status_bar_image_battery_level_3), \
                        ESP_UI_STYLE_IMAGE_RECOLOR_WHITE(&esp_ui_phone_status_bar_image_battery_level_4), \
                        ESP_UI_STYLE_IMAGE_RECOLOR_WHITE(&esp_ui_phone_status_bar_image_battery_charge),  \
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
                        ESP_UI_STYLE_IMAGE_RECOLOR_WHITE(&esp_ui_phone_status_bar_image_wifi_close),      \
                        ESP_UI_STYLE_IMAGE_RECOLOR_WHITE(&esp_ui_phone_status_bar_image_wifi_level_1),    \
                        ESP_UI_STYLE_IMAGE_RECOLOR_WHITE(&esp_ui_phone_status_bar_image_wifi_level_2),    \
                        ESP_UI_STYLE_IMAGE_RECOLOR_WHITE(&esp_ui_phone_status_bar_image_wifi_level_3),    \
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
