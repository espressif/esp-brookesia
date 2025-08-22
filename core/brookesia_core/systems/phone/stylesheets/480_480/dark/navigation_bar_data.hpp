/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/widgets/navigation_bar/esp_brookesia_navigation_bar.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"

namespace esp_brookesia::systems::phone {

constexpr NavigationBar::Data STYLESHEET_480_480_DARK_NAVIGATION_BAR_DATA = {
    .main = {
        .size = gui::StyleSize::RECT_W_PERCENT(100, 64),
        .background_color = gui::StyleColor::COLOR(0x38393A),
    },
    .button = {
        .icon_size = gui::StyleSize::SQUARE(32),
        .icon_images = {
            gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_middle_navigation_bar_back_32_32),
            gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_middle_navigation_bar_home_32_32),
            gui::StyleImage::IMAGE_RECOLOR_WHITE(&esp_brookesia_image_middle_navigation_bar_recents_screen_32_32),
        },
        .navigate_types = {
            base::Manager::NavigateType::BACK,
            base::Manager::NavigateType::HOME,
            base::Manager::NavigateType::RECENTS_SCREEN,
        },
        .active_background_color = gui::StyleColor::COLOR_WITH_OPACITY(0xFFFFFF, LV_OPA_50),
    },
    .visual_flex = {
        .show_animation = {
            .duration_ms = 200,
            .path_type = gui::StyleAnimation::ANIM_PATH_TYPE_EASE_OUT,
        },
        .hide_animation = {
            .duration_ms = 200,
            .path_type = gui::StyleAnimation::ANIM_PATH_TYPE_EASE_IN,
        },
        .hide_timer_period_ms = 2000,
    },
    .flags = {
        .enable_main_size_min = 0,
        .enable_main_size_max = 0,
    },
};

} // namespace esp_brookesia::systems::phone
