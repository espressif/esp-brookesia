/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_BROOKESIA_LV_ANIM_PATH_TYPE_LINEAR = 0,
    ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_IN,
    ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_OUT,
    ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_IN_OUT,
    ESP_BROOKESIA_LV_ANIM_PATH_TYPE_OVERSHOOT,
    ESP_BROOKESIA_LV_ANIM_PATH_TYPE_BOUNCE,
    ESP_BROOKESIA_LV_ANIM_PATH_TYPE_STEP,
    ESP_BROOKESIA_LV_ANIM_PATH_TYPE_MAX,
} ESP_Brookesia_LvAnimationPathType_t;

typedef bool (*ESP_Brookesia_LvLockCallback_t)(int timeout_ms);

typedef void (*ESP_Brookesia_LvUnlockCallback_t)(void);

#ifdef __cplusplus
}
#endif
