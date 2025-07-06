/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/widgets/navigation_bar/esp_brookesia_navigation_bar.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"

constexpr ESP_Brookesia_NavigationBarData_t ESP_BROOKESIA_PHONE_1280_800_DARK_NAVIGATION_BAR_DATA()
{
    return {
        .main = {
            .size = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(100, 80),
            .background_color = ESP_BROOKESIA_STYLE_COLOR(0x38393A),
        },
        .button = {
            .icon_size = ESP_BROOKESIA_STYLE_SIZE_SQUARE(36),
            .icon_images = {
                ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_navigation_bar_back_36_36),
                ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_navigation_bar_home_36_36),
                ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(&esp_brookesia_image_large_navigation_bar_recents_screen_36_36),
            },
            .navigate_types = {
                ESP_BROOKESIA_CORE_NAVIGATE_TYPE_BACK,
                ESP_BROOKESIA_CORE_NAVIGATE_TYPE_HOME,
                ESP_BROOKESIA_CORE_NAVIGATE_TYPE_RECENTS_SCREEN,
            },
            .active_background_color = ESP_BROOKESIA_STYLE_COLOR_WITH_OPACITY(0xFFFFFF, LV_OPA_50),
        },
        .visual_flex = {
            .show_animation_time_ms = 200,
            .show_animation_delay_ms = 0,
            .show_animation_path_type = ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_OUT,
            .show_duration_ms = 2000,
            .hide_animation_time_ms = 200,
            .hide_animation_delay_ms = 0,
            .hide_animation_path_type = ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_IN,
        },
        .flags = {
            .enable_main_size_min = 0,
            .enable_main_size_max = 0,
        },
    };
}
