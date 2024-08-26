/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    ESP_UI_StyleImage_t image;
    int id;
} ESP_UI_AppLauncherIconInfo_t;

typedef struct {
    struct {
        ESP_UI_StyleSize_t size;
        uint8_t layout_row_pad;
    } main;
    struct {
        ESP_UI_StyleSize_t default_size;
        ESP_UI_StyleSize_t press_size;
    } image;
    struct {
        ESP_UI_StyleFont_t text_font;
        ESP_UI_StyleColor_t text_color;
    } label;
} ESP_UI_AppLauncherIconData_t;

typedef struct {
    struct {
        uint16_t y_start;
        ESP_UI_StyleSize_t size;
    } main;
    struct {
        uint8_t default_num;
        ESP_UI_StyleSize_t size;
    } table;
    struct {
        ESP_UI_StyleSize_t main_size;
        uint8_t main_layout_column_pad;
        uint16_t main_layout_bottom_offset;
        ESP_UI_StyleSize_t spot_inactive_size;
        ESP_UI_StyleSize_t spot_active_size;
        ESP_UI_StyleColor_t spot_inactive_background_color;
        ESP_UI_StyleColor_t spot_active_background_color;
    } indicator;
    ESP_UI_AppLauncherIconData_t icon;
    struct {
        uint8_t enable_table_scroll_anim: 1;
    } flags;
} ESP_UI_AppLauncherData_t;

#ifdef __cplusplus
}
#endif
