/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "widgets/gesture/esp_brookesia_gesture.hpp"

namespace esp_brookesia::speaker {

constexpr GestureIndicatorBarData ESP_BROOKESIA_SPEAKER_360_360_DARK_GESTURE_LEFT_RIGHT_INDICATOR_BAR_DATA = {
    .main = {
        .size_min = ESP_BROOKESIA_STYLE_SIZE_RECT(6, 0),
        .size_max = ESP_BROOKESIA_STYLE_SIZE_RECT_H_PERCENT(6, 30),
        .radius = 5,
        .layout_pad_all = 1,
        .color = ESP_BROOKESIA_STYLE_COLOR(0x000000),
    },
    .indicator = {
        .radius = 5,
        .color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),
    },
    .animation = {
        .scale_back_path_type = ESP_BROOKESIA_LV_ANIM_PATH_TYPE_BOUNCE,
        .scale_back_time_ms = 500,
    },
};

constexpr GestureIndicatorBarData ESP_BROOKESIA_SPEAKER_360_360_DARK_GESTURE_BOTTOM_INDICATOR_BAR_DATA = {
    .main = {
        .size_min = ESP_BROOKESIA_STYLE_SIZE_RECT(0, 6),
        .size_max = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(30, 6),
        .radius = 5,
        .layout_pad_all = 1,
        .color = ESP_BROOKESIA_STYLE_COLOR(0x1A1A1A),
    },
    .indicator = {
        .radius = 5,
        .color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),
    },
    .animation = {
        .scale_back_path_type = ESP_BROOKESIA_LV_ANIM_PATH_TYPE_BOUNCE,
        .scale_back_time_ms = 500,
    },
};

constexpr GestureData ESP_BROOKESIA_SPEAKER_360_360_DARK_GESTURE_DATA = {
    .detect_period_ms = 20,
    .threshold = {
        .direction_vertical = 20,
        .direction_horizon = 20,
        .direction_angle = 60,
        .horizontal_edge = 20,
        .vertical_edge = 20,
        .duration_short_ms = 800,
        .speed_slow_px_per_ms = 0.1,
    },
    .indicator_bars = {
        ESP_BROOKESIA_SPEAKER_360_360_DARK_GESTURE_LEFT_RIGHT_INDICATOR_BAR_DATA,
        ESP_BROOKESIA_SPEAKER_360_360_DARK_GESTURE_LEFT_RIGHT_INDICATOR_BAR_DATA,
        ESP_BROOKESIA_SPEAKER_360_360_DARK_GESTURE_BOTTOM_INDICATOR_BAR_DATA,
    },
    .flags = {
        .enable_indicator_bars = {0, 0, 1},
    },
};

} // namespace esp_brookesia::speaker
