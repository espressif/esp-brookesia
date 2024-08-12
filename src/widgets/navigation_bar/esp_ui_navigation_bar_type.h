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
        uint8_t enable_main_size_min: 1;
        uint8_t enable_main_size_max: 1;
    } flags;
} ESP_UI_NavigationBarData_t;

typedef enum {
    ESP_UI_NAVIGATION_BAR_VISUAL_MODE_HIDE = 0,
    ESP_UI_NAVIGATION_BAR_VISUAL_MODE_SHOW_FIXED,
    // ESP_UI_NAVIGATION_BAR_VISUAL_MODE_SHOW_FLEX,  // TODO
} ESP_UI_NavigationBarVisualMode_t;

#ifdef __cplusplus
}
#endif
