/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "core_data.hpp"
#include "app_launcher_data.hpp"
#include "gesture_data.hpp"
#include "quick_settings.hpp"
#include "keyboard.hpp"
#include "assets/esp_brookesia_speaker_assets.h"
#include "esp_brookesia_speaker.hpp"

namespace esp_brookesia::speaker {

/* Display */
constexpr DisplayData ESP_BROOKESIA_SPEAKER_360_360_DARK_DISPLAY_DATA = {
    .boot_animation = {
        .data = {
            .canvas = {
                .coord_x = 0,
                .coord_y = 0,
                .width = 360,
                .height = 360,
            },
            .task = {
                .task_priority = 4,
                .task_stack = 10 * 1024,
                .task_affinity = 0,
                .task_stack_in_ext = true,
            },
            .source = {
                .animation_num = 1,
                .animation_configs = (const gui::AnimPlayerAnimConfig [])
                {
                    { .fps = 18, },
                },
                .partition_config = gui::AnimPlayerPartitionConfig{
                    .partition_label = "anim_boot",
                    .max_files = MMAP_BOOT_FILES,
                    .checksum = MMAP_BOOT_CHECKSUM,
                },
            },
            .flags = {
                .enable_source_partition = true,
                .enable_data_swap_bytes = true,
            },
        },
    },
    .app_launcher = {
        .data = ESP_BROOKESIA_SPEAKER_360_360_DARK_APP_LAUNCHER_DATA,
        .default_image = ESP_BROOKESIA_STYLE_IMAGE(&esp_brookesia_image_middle_app_launcher_default_112_112),
    },
    .quick_settings = {
        .data = ESP_BROOKESIA_SPEAKER_360_360_DARK_QUICK_SETTINGS_DATA,
    },
    .keyboard = {
        .data = ESP_BROOKESIA_SPEAKER_360_360_DARK_KEYBOARD_DATA,
    },
    .flags = {
        .enable_app_launcher_flex_size = 1,
    },
};

/* Manager */
constexpr ManagerData ESP_BROOKESIA_SPEAKER_360_360_DARK_MANAGER_DATA = {
    .gesture = ESP_BROOKESIA_SPEAKER_360_360_DARK_GESTURE_DATA,
    .gesture_mask_indicator_trigger_time_ms = 0,
    .ai_buddy_resume_time_ms = 5000,
    .quick_settings = {
        .top_threshold_percent = 20,
        .bottom_threshold_percent = 80,
    },
    .flags = {
        .enable_gesture = true,
        .enable_gesture_navigation_back = false,
        .enable_app_launcher_gesture_navigation = true,
        .enable_quick_settings_top_threshold_percent = true,
        .enable_quick_settings_bottom_threshold_percent = true,
    },
};

/* AI Agent */
constexpr AI_BuddyData ESP_BROOKESIA_SPEAKER_360_360_DARK_AI_AGENT_DATA = {
    .expression = {
        .data = {
            .emotion = {
                .data = {
                    .canvas = {
                        .coord_x = 38,
                        .coord_y = 80,
                        .width = 284,
                        .height = 126,
                    },
                    .task = {
                        .task_priority = 4,
                        .task_stack = 10 * 1024,
                        .task_affinity = 0,
                        .task_stack_in_ext = true,
                    },
                    .source = {
                        .animation_num = 6,
                        .animation_configs = (const gui::AnimPlayerAnimConfig [])
                        {
                            { .fps = 30, },
                            { .fps = 30, },
                            { .fps = 30, },
                            { .fps = 30, },
                            { .fps = 30, },
                            { .fps = 30, },
                        },
                        .partition_config = gui::AnimPlayerPartitionConfig{
                            .partition_label = "anim_emotion",
                            .max_files = MMAP_EMOTION_FILES,
                            .checksum = MMAP_EMOTION_CHECKSUM,
                        },
                    },
                    .flags = {
                        .enable_source_partition = true,
                        .enable_data_swap_bytes = true,
                    },
                },
            },
            .icon = {
                .data = {
                    .canvas = {
                        .coord_x = 148,
                        .coord_y = 6,
                        .width = 64,
                        .height = 64,
                    },
                    .task = {
                        .task_priority = 4,
                        .task_stack = 10 * 1024,
                        .task_affinity = 0,
                        .task_stack_in_ext = true,
                    },
                    .source = {
                        .animation_num = 11,
                        .animation_configs = (const gui::AnimPlayerAnimConfig [])
                        {
                            { .fps = 15, },
                            { .fps = 15, },
                            { .fps = 15, },
                            { .fps = 15, },
                            { .fps = 15, },
                            { .fps = 15, },
                            { .fps = 15, },
                            { .fps = 15, },
                            { .fps = 15, },
                            { .fps = 15, },
                            { .fps = 15, },
                        },
                        .partition_config = gui::AnimPlayerPartitionConfig{
                            .partition_label = "anim_icon",
                            .max_files = MMAP_ICON_FILES,
                            .checksum = MMAP_ICON_CHECKSUM,
                        },
                    },
                    .flags = {
                        .enable_source_partition = true,
                        .enable_data_swap_bytes = true,
                    },
                },
            },
            .flags = {
                .enable_emotion = true,
                .enable_icon = true,
            },
        },
    },
};

/* Speaker */
constexpr SpeakerStylesheet_t ESP_BROOKESIA_SPEAKER_360_360_DARK_STYLESHEET = {
    .core = ESP_BROOKESIA_SPEAKER_360_360_DARK_CORE_DATA,
    .display = ESP_BROOKESIA_SPEAKER_360_360_DARK_DISPLAY_DATA,
    .manager = ESP_BROOKESIA_SPEAKER_360_360_DARK_MANAGER_DATA,
    .ai_buddy = ESP_BROOKESIA_SPEAKER_360_360_DARK_AI_AGENT_DATA,
};

} // namespace esp_brookesia::speaker
