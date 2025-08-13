/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/widgets/status_bar/esp_brookesia_status_bar.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"

namespace esp_brookesia::systems::phone {

constexpr StatusBar::AreaData STYLESHEET_320_240_DARK_STATUS_BAR_AREA_DATA(int w_percent, StatusBar::AreaAlign align)
{
    return {
        .size = gui::StyleSize::RECT_PERCENT(w_percent, 100),
        .layout_column_align = align,
        .layout_column_start_offset = 12,
        .layout_column_pad = 3,
    };
}

constexpr StatusBar::Data STYLESHEET_320_240_DARK_STATUS_BAR_DATA = {
    .main = {
        .size = gui::StyleSize::RECT_W_PERCENT(100, 36),
        .size_min = {},
        .size_max = {},
        .background_color = gui::StyleColor::COLOR(0x38393A),
        .text_font = gui::StyleFont::HEIGHT_PERCENT(56),
        .text_color = gui::StyleColor::COLOR(0xFFFFFF),
    },
    .area = {
        .num = 2,
        .data = {
            STYLESHEET_320_240_DARK_STATUS_BAR_AREA_DATA(50, StatusBar::AreaAlign::START),
            STYLESHEET_320_240_DARK_STATUS_BAR_AREA_DATA(50, StatusBar::AreaAlign::END),
        },
    },
    .icon_common_size = gui::StyleSize::SQUARE_PERCENT(56),
    .battery = {
        .area_index = 1,
        .icon_data = {
            .icon = {
                .image_num = 5,
                .images = {
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_small_status_bar_battery_level1_20_20),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_small_status_bar_battery_level2_20_20),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_small_status_bar_battery_level3_20_20),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_small_status_bar_battery_level4_20_20),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_small_status_bar_battery_charge_20_20),
                },
            },
        },
    },
    .wifi = {
        .area_index = 1,
        .icon_data = {
            .icon = {
                .image_num = 4,
                .images = {
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_small_status_bar_wifi_close_20_20),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_small_status_bar_wifi_level1_20_20),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_small_status_bar_wifi_level2_20_20),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_small_status_bar_wifi_level3_20_20),
                },
            },
        },
    },
    .clock = {
        .area_index = 0,
    },
    .flags = {
        .enable_main_size_min = 0,
        .enable_main_size_max = 0,
        .enable_battery_icon = 1,
        .enable_battery_icon_common_size = 1,
        .enable_battery_label = 0,
        .enable_wifi_icon = 1,
        .enable_wifi_icon_common_size = 1,
        .enable_clock = 1,
    },
};

} // namespace esp_brookesia::systems::phone
