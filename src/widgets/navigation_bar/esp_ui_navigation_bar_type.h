/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "core/esp_ui_core_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_UI_NAVIGATION_BAR_DATA_BUTTON_NUM   (ESP_UI_CORE_NAVIGATE_TYPE_MAX)

typedef struct {
    struct {
        ESP_UI_StyleSize_t size;
        ESP_UI_StyleSize_t size_min;
        ESP_UI_StyleSize_t size_max;
        ESP_UI_StyleColor_t background_color;
    } main;
    struct {
        ESP_UI_StyleSize_t icon_size;
        ESP_UI_StyleImage_t icon_images[ESP_UI_NAVIGATION_BAR_DATA_BUTTON_NUM];
        ESP_UI_CoreNavigateType_t navigate_types[ESP_UI_NAVIGATION_BAR_DATA_BUTTON_NUM];
        ESP_UI_StyleColor_t active_background_color;
    } button;
    struct {
        uint32_t show_animation_time_ms;
        uint32_t show_animation_delay_ms;
        ESP_UI_LvAnimationPathType_t show_animation_path_type;
        uint32_t show_duration_ms;
        uint32_t hide_animation_time_ms;
        uint32_t hide_animation_delay_ms;
        ESP_UI_LvAnimationPathType_t hide_animation_path_type;
    } visual_flex;
    struct {
        uint8_t enable_main_size_min: 1;
        uint8_t enable_main_size_max: 1;
    } flags;
} ESP_UI_NavigationBarData_t;

typedef enum {
    ESP_UI_NAVIGATION_BAR_VISUAL_MODE_HIDE = 0,
    ESP_UI_NAVIGATION_BAR_VISUAL_MODE_SHOW_FIXED,
    ESP_UI_NAVIGATION_BAR_VISUAL_MODE_SHOW_FLEX,
    ESP_UI_NAVIGATION_BAR_VISUAL_MODE_MAX,
} ESP_UI_NavigationBarVisualMode_t;

#ifdef __cplusplus
}
#endif
