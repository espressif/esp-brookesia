/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_brookesia_systems_internal.h"
#include "assets/esp_brookesia_speaker_assets.h"
#include "esp_brookesia_speaker.hpp"
#include "core_data.hpp"
#include "app_launcher_data.hpp"
#include "gesture_data.hpp"
#include "quick_settings.hpp"
#include "keyboard.hpp"

namespace esp_brookesia::systems::speaker {

/* Display */
constexpr Display::Data STYLESHEET_360_360_DARK_DISPLAY_DATA = {
    .boot_animation = {
        .data = {
            .canvas = {
                .coord_x = 0,
                .coord_y = 0,
                .width = 360,
                .height = 360,
            },
            .source = gui::AnimPlayerPartitionConfig{
                .partition_label = "anim_boot",
                .max_files = MMAP_BOOT_FILES,
                .fps = (const int []) { 18 },
                .checksum = MMAP_BOOT_CHECKSUM,
            },
            // .source = gui::AnimPlayerResourcesConfig{
            //     .num = 1,
            //     .resources = (const gui::AnimPlayerAnimPath [])
            //     {
            //         {
            //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/boot_animation_360_360.aaf",
            //             .fps = 18,
            //         },
            //     },
            // },
            .task = {
                .task_priority = 4,
                .task_stack = 10 * 1024,
                .task_affinity = 0,
                .task_stack_in_ext = true,
            },
            .flags = {
                .enable_data_swap_bytes = true,
            },
        },
    },
    .app_launcher = {
        .data = STYLESHEET_360_360_DARK_APP_LAUNCHER_DATA,
        .default_image = gui::StyleImage::IMAGE(&speaker_image_middle_app_launcher_default_112_112),
    },
    .quick_settings = {
        .data = STYLESHEET_360_360_DARK_QUICK_SETTINGS_DATA,
    },
    .keyboard = {
        .data = STYLESHEET_360_360_DARK_KEYBOARD_DATA,
    },
    .flags = {
        .enable_app_launcher_flex_size = 1,
    },
};

/* Manager */
constexpr Manager::Data STYLESHEET_360_360_DARK_MANAGER_DATA = {
    .gesture = STYLESHEET_360_360_DARK_GESTURE_DATA,
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

/* AI Buddy */
constexpr AI_Buddy::Data STYLESHEET_360_360_DARK_AI_BUDDY_DATA = {
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
                    .source = gui::AnimPlayerPartitionConfig{
                        .partition_label = "anim_emotion",
                        .max_files = MMAP_EMOTION_FILES,
                        .fps = (const int []) { 30, 30, 30, 30, 30, 30, 30, 30},
                        .checksum = MMAP_EMOTION_CHECKSUM,
                    },
                    // .source = gui::AnimPlayerResourcesConfig{
                    //     .num = 6,
                    //     .resources = (const gui::AnimPlayerAnimPath [])
                    //     {
                    //         {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/emotion_angry_284_126.aaf",
                    //             .fps = 30,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/emotion_blink_fast_284_126.aaf",
                    //             .fps = 30,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/emotion_blink_slow_284_126.aaf",
                    //             .fps = 30,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/emotion_happy_284_126.aaf",
                    //             .fps = 30,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/emotion_sad_284_126.aaf",
                    //             .fps = 30,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/emotion_sleep_284_126.aaf",
                    //             .fps = 30,
                    //         },
                    //     },
                    // },
                    .task = {
                        .task_priority = 4,
                        .task_stack = 10 * 1024,
                        .task_affinity = 0,
                        .task_stack_in_ext = true,
                    },
                    .flags = {
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
                    .source = gui::AnimPlayerPartitionConfig{
                        .partition_label = "anim_icon",
                        .max_files = MMAP_ICON_FILES,
                        .fps = (const int []) { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15 },
                        .checksum = MMAP_ICON_CHECKSUM,
                    },
                    // .source = gui::AnimPlayerResourcesConfig{
                    //     .num = 11,
                    //     .resources = (const gui::AnimPlayerAnimPath [])
                    //     {
                    //         {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/icon_brightness_down_64.aaf",
                    //             .fps = 15,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/icon_brightness_up_64.aaf",
                    //             .fps = 15,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/icon_emotion_confused_64.aaf",
                    //             .fps = 15,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/icon_emotion_sleep_64.aaf",
                    //             .fps = 15,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/icon_emotion_thinking_64.aaf",
                    //             .fps = 15,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/icon_server_connected_64.aaf",
                    //             .fps = 15,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/icon_server_connecting_64.aaf",
                    //             .fps = 15,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/icon_volume_down_64.aaf",
                    //             .fps = 15,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/icon_volume_mute_64.aaf",
                    //             .fps = 15,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/icon_volume_up_64.aaf",
                    //             .fps = 15,
                    //         }, {
                    //             .path = STYLESHEET_FS_MOUNT_POINT "/system/animations/icon_wifi_disconnect_64.aaf",
                    //             .fps = 15,
                    //         },
                    //     },
                    // },
                    .task = {
                        .task_priority = 4,
                        .task_stack = 10 * 1024,
                        .task_affinity = 0,
                        .task_stack_in_ext = true,
                    },
                    .flags = {
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
constexpr Stylesheet STYLESHEET_360_360_DARK_STYLESHEET = {
    .core = STYLESHEET_360_360_DARK_CORE_DATA,
    .display = STYLESHEET_360_360_DARK_DISPLAY_DATA,
    .manager = STYLESHEET_360_360_DARK_MANAGER_DATA,
    .ai_buddy = STYLESHEET_360_360_DARK_AI_BUDDY_DATA,
};

} // namespace esp_brookesia::systems::speaker

#define ESP_BROOKESIA_SPEAKER_360_360_DARK_STYLESHEET  esp_brookesia::systems::speaker::STYLESHEET_360_360_DARK_STYLESHEET
