/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "widgets/gesture/esp_brookesia_gesture_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_BROOKESIA_PHONE_800_480_DRAK_GESTURE_LEFT_RIGHT_INDICATOR_BAR_DATA() \
    {                                                                     \
        .main = {                                                         \
            .size_min = ESP_BROOKESIA_STYLE_SIZE_RECT(10, 0),                    \
            .size_max = ESP_BROOKESIA_STYLE_SIZE_RECT_H_PERCENT(10, 50),         \
            .radius = 5,                                                  \
            .layout_pad_all = 2,                                          \
            .color = ESP_BROOKESIA_STYLE_COLOR(0x000000),                        \
        },                                                                \
        .indicator = {                                                    \
            .radius = 5,                                                  \
            .color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),                        \
        },                                                                \
        .animation = {                                                    \
            .scale_back_path_type = ESP_BROOKESIA_LV_ANIM_PATH_TYPE_BOUNCE,      \
            .scale_back_time_ms = 500,                                    \
        },                                                                \
    }

#define ESP_BROOKESIA_PHONE_800_480_DRAK_GESTURE_BOTTOM_INDICATOR_BAR_DATA() \
    {                                                                 \
        .main = {                                                     \
            .size_min = ESP_BROOKESIA_STYLE_SIZE_RECT(0, 10),                \
            .size_max = ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(50, 10),     \
            .radius = 5,                                              \
            .layout_pad_all = 2,                                      \
            .color = ESP_BROOKESIA_STYLE_COLOR(0x1A1A1A),                    \
        },                                                            \
        .indicator = {                                                \
            .radius = 5,                                              \
            .color = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF),                    \
        },                                                            \
        .animation = {                                                \
            .scale_back_path_type = ESP_BROOKESIA_LV_ANIM_PATH_TYPE_BOUNCE,  \
            .scale_back_time_ms = 500,                                \
        },                                                            \
    }

#define ESP_BROOKESIA_PHONE_800_480_DARK_GESTURE_DATA()                                   \
    {                                                                              \
        .detect_period_ms = 20,                                                    \
        .threshold = {                                                             \
            .direction_vertical = 50,                                              \
            .direction_horizon = 50,                                               \
            .direction_angle = 60,                                                 \
            .horizontal_edge = 10,                                                        \
            .vertical_edge = 20,                                                     \
            .duration_short_ms = 800,                                              \
            .speed_slow_px_per_ms = 0.1,                                           \
        },                                                                         \
        .indicator_bars = {                                                        \
            [ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_LEFT] =                             \
                ESP_BROOKESIA_PHONE_800_480_DRAK_GESTURE_LEFT_RIGHT_INDICATOR_BAR_DATA(), \
            [ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_RIGHT] =                            \
                ESP_BROOKESIA_PHONE_800_480_DRAK_GESTURE_LEFT_RIGHT_INDICATOR_BAR_DATA(), \
            [ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_BOTTOM] =                           \
                ESP_BROOKESIA_PHONE_800_480_DRAK_GESTURE_BOTTOM_INDICATOR_BAR_DATA(),     \
        },                                                                         \
        .flags = {                                                                 \
            .enable_indicator_bars = {                                             \
                [ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_LEFT] = 0,                      \
                [ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_RIGHT] = 0,                     \
                [ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_BOTTOM] = 1,                    \
            },                                                                     \
        },                                                                         \
    }

#ifdef __cplusplus
}
#endif
