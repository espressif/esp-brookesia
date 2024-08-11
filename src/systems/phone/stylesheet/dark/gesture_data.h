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

#define ESP_UI_PHONE_DEFAULT_DARK_GESTURE_DATA() \
    {                                            \
        .detect_period_ms = 40,                  \
        .threshold = {                           \
            .direction_vertical = 50,            \
            .direction_horizon = 50,             \
            .direction_angle = 60,               \
            .top_edge = 50,                      \
            .bottom_edge = 50,                   \
            .left_edge = 50,                     \
            .right_edge = 50,                    \
            .duration_short_ms = 500,            \
            .speed_slow_px_per_ms = 0.1,         \
        },                                       \
    }

#ifdef __cplusplus
}
#endif
