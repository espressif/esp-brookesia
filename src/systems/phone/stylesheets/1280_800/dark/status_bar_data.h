/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "widgets/status_bar/esp_brookesia_status_bar_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Icon Images */
// Battery
LV_IMG_DECLARE(esp_brookesia_phone_status_bar_battery_image_level_1);
LV_IMG_DECLARE(esp_brookesia_phone_status_bar_battery_image_level_2);
LV_IMG_DECLARE(esp_brookesia_phone_status_bar_battery_image_level_3);
LV_IMG_DECLARE(esp_brookesia_phone_status_bar_battery_image_level_4);
LV_IMG_DECLARE(esp_brookesia_phone_status_bar_battery_image_charge);
// WiFi
LV_IMG_DECLARE(esp_brookesia_phone_status_bar_image_wifi_close);
LV_IMG_DECLARE(esp_brookesia_phone_status_bar_image_wifi_level_1);
LV_IMG_DECLARE(esp_brookesia_phone_status_bar_image_wifi_level_2);
LV_IMG_DECLARE(esp_brookesia_phone_status_bar_image_wifi_level_3);

/* Area */
#define ESP_BROOKESIA_PHONE_1280_800_DARK_STATUS_BAR_AREA_DATA(w_percent, align) \
    {                                                                     \
        .size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(w_percent, 100),           \
        .layout_column_align = align,                                     \
        .layout_column_start_offset = 40,                                 \
        .layout_column_pad = 8,                                           \
    }

/* Status Bar */
#define ESP_BROOKESIA_PHONE_1280_800_DARK_STATUS_BAR_DATA()                                                      \
    {                                                                                                     \
        .main = {                                                                                         \
            .size = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(100, 50),                                            \
            .background_color = ESP_BROOKESIA_STYLE_COLOR(0x38393A),                                             \
            .text_font = ESP_BROOKESIA_STYLE_FONT_SIZE(22),                                                      \
            .text_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),                                                   \
        },                                                                                                \
        .area = {                                                                                         \
            .num = ESP_BROOKESIA_STATUS_BAR_DATA_AREA_NUM_MAX,                                                   \
            .data = {                                                                                     \
                ESP_BROOKESIA_PHONE_1280_800_DARK_STATUS_BAR_AREA_DATA(33, ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_START),  \
                ESP_BROOKESIA_PHONE_1280_800_DARK_STATUS_BAR_AREA_DATA(34, ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_CENTER), \
                ESP_BROOKESIA_PHONE_1280_800_DARK_STATUS_BAR_AREA_DATA(33, ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_END),    \
            },                                                                                            \
        },                                                                                                \
        .icon_common_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(28),                                                 \
        .battery = {                                                                                      \
            .area_index = ESP_BROOKESIA_STATUS_BAR_DATA_AREA_NUM_MAX - 1,                                        \
            .icon_data = {                                                                                \
                .icon = {                                                                                 \
                    .image_num = 5,                                                                       \
                    .images = {                                                                           \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_phone_status_bar_battery_image_level_1), \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_phone_status_bar_battery_image_level_2), \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_phone_status_bar_battery_image_level_3), \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_phone_status_bar_battery_image_level_4), \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_phone_status_bar_battery_image_charge),  \
                    },                                                                                    \
                },                                                                                        \
            },                                                                                            \
        },                                                                                                \
        .wifi = {                                                                                         \
            .area_index = ESP_BROOKESIA_STATUS_BAR_DATA_AREA_NUM_MAX - 1,                                        \
            .icon_data = {                                                                                \
                .icon = {                                                                                 \
                    .image_num = 4,                                                                       \
                    .images = {                                                                           \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_phone_status_bar_image_wifi_close),      \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_phone_status_bar_image_wifi_level_1),    \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_phone_status_bar_image_wifi_level_2),    \
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_phone_status_bar_image_wifi_level_3),    \
                    },                                                                                    \
                },                                                                                        \
            },                                                                                            \
        },                                                                                                \
        .clock = {                                                                                        \
            .area_index = 1,                                                                              \
        },                                                                                                \
        .flags = {                                                                                        \
            .enable_battery_icon = 1,                                                                     \
            .enable_battery_icon_common_size = 1,                                                         \
            .enable_battery_label = 1,                                                                    \
            .enable_wifi_icon = 1,                                                                        \
            .enable_wifi_icon_common_size = 1,                                                            \
            .enable_clock = 1,                                                                            \
        },                                                                                                \
    }

#ifdef __cplusplus
}
#endif