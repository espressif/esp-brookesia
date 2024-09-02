/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "widgets/gesture/esp_ui_gesture_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_UI_PHONE_DEFAULT_DARK_GESTURE_LEFT_RIGHT_INDICATOR_BAR_DATA() \
    {                                                                     \
        .main = {                                                         \
            .size_min = ESP_UI_STYLE_SIZE_RECT(10, 4),                    \
            .size_max = ESP_UI_STYLE_SIZE_RECT_H_PERCENT(10, 50),         \
            .radius = 5,                                                  \
            .align_offset = 0,                                            \
            .layout_pad_all = 2,                                          \
            .color = ESP_UI_STYLE_COLOR(0x000000),                        \
        },                                                                \
        .indicator = {                                                    \
            .radius = 5,                                                  \
            .color = ESP_UI_STYLE_COLOR(0xFFFFFF),                        \
        },                                                                \
        .animation = {                                                    \
            .scale_back_path_type = ESP_UI_LV_ANIM_PATH_TYPE_BOUNCE,      \
            .scale_back_time_ms = 500,                                    \
        },                                                                \
    }

#define ESP_UI_PHONE_DEFAULT_DARK_GESTURE_BOTTOM_INDICATOR_BAR_DATA() \
    {                                                                 \
        .main = {                                                     \
            .size_min = ESP_UI_STYLE_SIZE_RECT(4, 10),                \
            .size_max = ESP_UI_STYLE_SIZE_RECT_W_PERCENT(50, 10),     \
            .radius = 5,                                              \
            .align_offset = 0,                                        \
            .layout_pad_all = 2,                                      \
            .color = ESP_UI_STYLE_COLOR(0x1A1A1A),                    \
        },                                                            \
        .indicator = {                                                \
            .radius = 5,                                              \
            .color = ESP_UI_STYLE_COLOR(0xFFFFFF),                    \
        },                                                            \
        .animation = {                                                \
            .scale_back_path_type = ESP_UI_LV_ANIM_PATH_TYPE_BOUNCE,  \
            .scale_back_time_ms = 500,                                \
        },                                                            \
    }

#define ESP_UI_PHONE_DEFAULT_DARK_GESTURE_DATA()                                   \
    {                                                                              \
        .detect_period_ms = 20,                                                    \
        .threshold = {                                                             \
            .direction_vertical = 50,                                              \
            .direction_horizon = 50,                                               \
            .direction_angle = 60,                                                 \
            .top_edge = 50,                                                        \
            .bottom_edge = 50,                                                     \
            .left_edge = 50,                                                       \
            .right_edge = 50,                                                      \
            .duration_short_ms = 500,                                              \
            .speed_slow_px_per_ms = 0.1,                                           \
        },                                                                         \
        .indicator_bars = {                                                        \
            [ESP_UI_GESTURE_INDICATOR_BAR_TYPE_LEFT] =                             \
                ESP_UI_PHONE_DEFAULT_DARK_GESTURE_LEFT_RIGHT_INDICATOR_BAR_DATA(), \
            [ESP_UI_GESTURE_INDICATOR_BAR_TYPE_RIGHT] =                            \
                ESP_UI_PHONE_DEFAULT_DARK_GESTURE_LEFT_RIGHT_INDICATOR_BAR_DATA(), \
            [ESP_UI_GESTURE_INDICATOR_BAR_TYPE_BOTTOM] =                           \
                ESP_UI_PHONE_DEFAULT_DARK_GESTURE_BOTTOM_INDICATOR_BAR_DATA(),     \
        },                                                                         \
        .flags = {                                                                 \
            .enable_indicator_bars = {                                             \
                [ESP_UI_GESTURE_INDICATOR_BAR_TYPE_LEFT] = 1,                      \
                [ESP_UI_GESTURE_INDICATOR_BAR_TYPE_RIGHT] = 1,                     \
                [ESP_UI_GESTURE_INDICATOR_BAR_TYPE_BOTTOM] = 1,                    \
            },                                                                     \
        },                                                                         \
    }

#ifdef __cplusplus
}
#endif
