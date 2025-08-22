/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/widgets/status_bar/esp_brookesia_status_bar.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"

namespace esp_brookesia::systems::phone {

constexpr StatusBar::AreaData STYLESHEET_800_1280_DARK_STATUS_BAR_AREA_DATA(int w_percent, StatusBar::AreaAlign align)
{
    return {
        .size = gui::StyleSize::RECT_PERCENT(w_percent, 100),
        .layout_column_align = align,
        .layout_column_start_offset = 40,
        .layout_column_pad = 8,
    };
}

constexpr StatusBar::Data STYLESHEET_800_1280_DARK_STATUS_BAR_DATA = {
    .main = {
        .size = gui::StyleSize::RECT_W_PERCENT(100, 50),
        .background_color = gui::StyleColor::COLOR(0x38393A),
        .text_font = gui::StyleFont::SIZE(22),
        .text_color = gui::StyleColor::COLOR(0xFFFFFF),
    },
    .area = {
        .num = StatusBar::AREA_NUM_MAX,
        .data = {
            STYLESHEET_800_1280_DARK_STATUS_BAR_AREA_DATA(33, StatusBar::AreaAlign::START),
            STYLESHEET_800_1280_DARK_STATUS_BAR_AREA_DATA(34, StatusBar::AreaAlign::CENTER),
            STYLESHEET_800_1280_DARK_STATUS_BAR_AREA_DATA(33, StatusBar::AreaAlign::END),
        },
    },
    .icon_common_size = gui::StyleSize::SQUARE(28),
    .battery = {
        .area_index = StatusBar::AREA_NUM_MAX - 1,
        .icon_data = {
            .icon = {
                .image_num = 5,
                .images = {
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_level1_36_36),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_level2_36_36),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_level3_36_36),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_level4_36_36),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_battery_charge_36_36),
                },
            },
        },
    },
    .wifi = {
        .area_index = StatusBar::AREA_NUM_MAX - 1,
        .icon_data = {
            .icon = {
                .image_num = 4,
                .images = {
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_close_36_36),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_level1_36_36),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_level2_36_36),
                    gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_status_bar_wifi_level3_36_36),
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

} // namespace esp_brookesia::systems::phone
