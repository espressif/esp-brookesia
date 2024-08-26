/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_UI_LV_ANIM_PATH_TYPE_LINEAR = 0,
    ESP_UI_LV_ANIM_PATH_TYPE_EASE_IN,
    ESP_UI_LV_ANIM_PATH_TYPE_EASE_OUT,
    ESP_UI_LV_ANIM_PATH_TYPE_EASE_IN_OUT,
    ESP_UI_LV_ANIM_PATH_TYPE_OVERSHOOT,
    ESP_UI_LV_ANIM_PATH_TYPE_BOUNCE,
    ESP_UI_LV_ANIM_PATH_TYPE_STEP,
    ESP_UI_LV_ANIM_PATH_TYPE_MAX,
} ESP_UI_LvAnimationPathType_t;

#ifdef __cplusplus
}
#endif
