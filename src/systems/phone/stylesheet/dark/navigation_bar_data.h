/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "widgets/navigation_bar/esp_ui_navigation_bar_type.h"

#ifdef __cplusplus
extern "C" {
#endif

LV_IMG_DECLARE(esp_ui_phone_navigation_bar_image_back);
LV_IMG_DECLARE(esp_ui_phone_navigation_bar_image_home);
LV_IMG_DECLARE(esp_ui_phone_navigation_bar_image_recents_screen);

#define ESP_UI_PHONE_DEFAULT_DARK_NAVIGATION_BAR_DATA()                                              \
    {                                                                                                \
        .main = {                                                                                    \
            .size = ESP_UI_STYLE_SIZE_RECT_PERCENT(100, 15),                                         \
            .size_min = ESP_UI_STYLE_SIZE_RECT_W_PERCENT(100, 36),                                   \
            .size_max = ESP_UI_STYLE_SIZE_RECT_W_PERCENT(100, 60),                                   \
            .background_color = ESP_UI_STYLE_COLOR(0x38393A),                                        \
        },                                                                                           \
        .button = {                                                                                  \
            .icon_size = ESP_UI_STYLE_SIZE_SQUARE_PERCENT(50),                                       \
            .icon_images = {                                                                         \
                ESP_UI_STYLE_IMAGE_RECOLOR_WHITE(&esp_ui_phone_navigation_bar_image_back),           \
                ESP_UI_STYLE_IMAGE_RECOLOR_WHITE(&esp_ui_phone_navigation_bar_image_home),           \
                ESP_UI_STYLE_IMAGE_RECOLOR_WHITE(&esp_ui_phone_navigation_bar_image_recents_screen), \
            },                                                                                       \
            .navigate_types = {                                                                      \
                ESP_UI_CORE_NAVIGATE_TYPE_BACK,                                                      \
                ESP_UI_CORE_NAVIGATE_TYPE_HOME,                                                      \
                ESP_UI_CORE_NAVIGATE_TYPE_RECENTS_SCREEN,                                            \
            },                                                                                       \
            .active_background_color = ESP_UI_STYLE_COLOR_WITH_OPACIRY(0xFFFFFF, LV_OPA_50),         \
        },                                                                                           \
        .visual_flex = {                                                                             \
            .show_animation_time_ms = 200,                                                           \
            .show_animation_delay_ms = 0,                                                            \
            .show_animation_path_type = ESP_UI_LV_ANIM_PATH_TYPE_EASE_OUT,                           \
            .show_duration_ms = 2000,                                                                \
            .hide_animation_time_ms = 200,                                                           \
            .hide_animation_delay_ms = 0,                                                            \
            .hide_animation_path_type = ESP_UI_LV_ANIM_PATH_TYPE_EASE_IN,                            \
        },                                                                                           \
        .flags = {                                                                                   \
            .enable_main_size_min = 1,                                                               \
            .enable_main_size_max = 1,                                                               \
        },                                                                                           \
    }

#ifdef __cplusplus
}
#endif
