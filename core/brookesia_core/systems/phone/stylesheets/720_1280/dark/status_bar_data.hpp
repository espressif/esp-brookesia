/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/widgets/status_bar/esp_brookesia_status_bar.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"

constexpr ESP_Brookesia_StatusBarAreaData_t ESP_BROOKESIA_PHONE_720_1280_DARK_STATUS_BAR_AREA_DATA(int w_percent, ESP_Brookesia_StatusBarAreaAlign_t align)
{
    return {
        .size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(w_percent, 100),
        .layout_column_align = align,
        .layout_column_start_offset = 40,
        .layout_column_pad = 8,
    };
}

constexpr ESP_Brookesia_StatusBarData_t ESP_BROOKESIA_PHONE_720_1280_DARK_STATUS_BAR_DATA()
{
    return {
        .main = {
            .size = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(100, 60),
            .background_color = ESP_BROOKESIA_STYLE_COLOR(0x38393A),
            .text_font = ESP_BROOKESIA_STYLE_FONT_SIZE(28),
            .text_color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),
        },
        .area = {
            .num = ESP_BROOKESIA_STATUS_BAR_DATA_AREA_NUM_MAX,
            .data = {
                ESP_BROOKESIA_PHONE_720_1280_DARK_STATUS_BAR_AREA_DATA(33, ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_START),
                ESP_BROOKESIA_PHONE_720_1280_DARK_STATUS_BAR_AREA_DATA(34, ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_CENTER),
                ESP_BROOKESIA_PHONE_720_1280_DARK_STATUS_BAR_AREA_DATA(33, ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_END),
            },
        },
        .icon_common_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(36),
        .battery = {
            .area_index = ESP_BROOKESIA_STATUS_BAR_DATA_AREA_NUM_MAX - 1,
            .icon_data = {
                .icon = {
                    .image_num = 5,
                    .images = {
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_level1_36_36),
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_level2_36_36),
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_level3_36_36),
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_level4_36_36),
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_charge_36_36),
                    },
                },
            },
        },
        .wifi = {
            .area_index = ESP_BROOKESIA_STATUS_BAR_DATA_AREA_NUM_MAX - 1,
            .icon_data = {
                .icon = {
                    .image_num = 4,
                    .images = {
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_close_36_36),
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_level1_36_36),
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_level2_36_36),
                        ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_level3_36_36),
                    },
                },
            },
        },
        .clock = {
            .area_index = 1,
        },
        .flags = {
            .enable_battery_icon = 1,
            .enable_battery_icon_common_size = 1,
            .enable_battery_label = 1,
            .enable_wifi_icon = 1,
            .enable_wifi_icon_common_size = 1,
            .enable_clock = 1,
        },
    };
}
