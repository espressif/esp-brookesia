/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "core_data.hpp"
#include "app_launcher_data.hpp"
#include "gesture_data.hpp"
#include "assets/esp_brookesia_speaker_assets.h"
#include "esp_brookesia_speaker.hpp"

namespace esp_brookesia::speaker {

/* Display */
constexpr DisplayData ESP_BROOKESIA_SPEAKER_DEFAULT_DARK_DISPLAY_DATA = {
    .app_launcher = {
        .data = ESP_BROOKESIA_SPEAKER_DEFAULT_DARK_APP_LAUNCHER_DATA,
        .default_image = ESP_BROOKESIA_STYLE_IMAGE(&speaker_image_middle_app_launcher_default_112_112),
    },
    .flags = {
        .enable_app_launcher_flex_size = 1,
    },
};

/* Manager */
constexpr ManagerData ESP_BROOKESIA_SPEAKER_DEFAULT_DARK_MANAGER_DATA = {
    .gesture = ESP_BROOKESIA_SPEAKER_DEFAULT_DARK_GESTURE_DATA,
    .gesture_mask_indicator_trigger_time_ms = 0,
    .flags = {
        .enable_gesture = 1,
        .enable_gesture_navigation_back = 0,
    },
};

/* Speaker */
constexpr SpeakerStylesheet_t ESP_BROOKESIA_SPEAKER_DEFAULT_DARK_STYLESHEET = {
    .core = ESP_BROOKESIA_SPEAKER_DEFAULT_DARK_CORE_DATA,
    .display = ESP_BROOKESIA_SPEAKER_DEFAULT_DARK_DISPLAY_DATA,
    .manager = ESP_BROOKESIA_SPEAKER_DEFAULT_DARK_MANAGER_DATA,
};

} // namespace esp_brookesia::speaker
