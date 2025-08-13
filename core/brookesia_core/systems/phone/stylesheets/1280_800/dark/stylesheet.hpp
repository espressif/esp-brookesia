/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/esp_brookesia_phone.hpp"
#include "core_data.hpp"
#include "app_launcher_data.hpp"
#include "recents_screen_data.hpp"
#include "gesture_data.hpp"
#include "navigation_bar_data.hpp"
#include "status_bar_data.hpp"

namespace esp_brookesia::systems::phone {

constexpr Display::Data STYLESHEET_1280_800_DARK_DISPLAY_DATA = {
    .status_bar = {
        .data = STYLESHEET_1280_800_DARK_STATUS_BAR_DATA,
        .visual_mode = StatusBar::VisualMode::SHOW_FIXED,
    },
    .navigation_bar = {
        .data = STYLESHEET_1280_800_DARK_NAVIGATION_BAR_DATA,
        .visual_mode = NavigationBar::VisualMode::HIDE,
    },
    .app_launcher = {
        .data = STYLESHEET_1280_800_DARK_APP_LAUNCHER_DATA,
        .default_image = gui::StyleImage::IMAGE(&esp_brookesia_image_large_app_launcher_default_112_112),
    },
    .recents_screen = {
        .data = STYLESHEET_1280_800_DARK_RECENTS_SCREEN_DATA,
        .status_bar_visual_mode = StatusBar::VisualMode::HIDE,
        .navigation_bar_visual_mode = NavigationBar::VisualMode::HIDE,
    },
    .flags = {
        .enable_status_bar = 1,
        .enable_navigation_bar = 1,
        .enable_app_launcher_flex_size = 1,
        .enable_recents_screen = 1,
        .enable_recents_screen_flex_size = 1,
    },
};

constexpr Manager::Data STYLESHEET_1280_800_DARK_MANAGER_DATA = {
    .gesture = STYLESHEET_1280_800_DARK_GESTURE_DATA,
    .gesture_mask_indicator_trigger_time_ms = 0,
    .recents_screen = {
        .drag_snapshot_y_step = 10,
        .drag_snapshot_y_threshold = 50,
        .drag_snapshot_angle_threshold = 60,
        .delete_snapshot_y_threshold = 50,
    },
    .flags = {
        .enable_gesture = 1,
        .enable_gesture_navigation_back = 0,
        .enable_recents_screen_snapshot_drag = 1,
        .enable_recents_screen_hide_when_no_snapshot = 1,
    },
};

constexpr Stylesheet STYLESHEET_1280_800_DARK = {
    .core = STYLESHEET_1280_800_DARK_CORE_DATA,
    .display = STYLESHEET_1280_800_DARK_DISPLAY_DATA,
    .manager = STYLESHEET_1280_800_DARK_MANAGER_DATA,
};

} // namespace esp_brookesia::systems::phone

#define ESP_BROOKESIA_PHONE_1280_800_DARK_STYLESHEET() esp_brookesia::systems::phone::STYLESHEET_1280_800_DARK
