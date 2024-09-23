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
    const void *icon_image_resource;
    const void *snapshot_image_resource;
    int id;
} ESP_Brookesia_RecentsScreenSnapshotConf_t;

typedef struct {
    ESP_Brookesia_StyleSize_t main_size;
    struct {
        ESP_Brookesia_StyleSize_t main_size;
        uint8_t main_layout_column_pad;
        ESP_Brookesia_StyleSize_t icon_size;
        ESP_Brookesia_StyleFont_t text_font;
        ESP_Brookesia_StyleColor_t text_color;
    } title;
    struct {
        ESP_Brookesia_StyleSize_t main_size;
        uint8_t radius;
    } image;
    struct {
        uint8_t enable_all_main_size_refer_screen: 1;
    } flags;
} ESP_Brookesia_RecentsScreenSnapshotData_t;

typedef struct {
    struct {
        uint16_t y_start;
        ESP_Brookesia_StyleSize_t size;
        uint16_t layout_row_pad;
        uint16_t layout_top_pad;
        uint16_t layout_bottom_pad;
        ESP_Brookesia_StyleColor_t background_color;
    } main;
    struct {
        ESP_Brookesia_StyleSize_t main_size;
        uint8_t main_layout_x_right_offset;
        ESP_Brookesia_StyleFont_t label_text_font;
        ESP_Brookesia_StyleColor_t label_text_color;
        const char *label_unit_text;
    } memory;
    struct {
        ESP_Brookesia_StyleSize_t main_size;
        uint16_t main_layout_column_pad;
        ESP_Brookesia_RecentsScreenSnapshotData_t snapshot;
    } snapshot_table;
    struct {
        ESP_Brookesia_StyleSize_t default_size;
        ESP_Brookesia_StyleSize_t press_size;
        ESP_Brookesia_StyleImage_t image;
    } trash_icon;
    struct {
        uint8_t enable_memory: 1;
        uint8_t enable_table_height_flex: 1;
        uint8_t enable_table_snapshot_use_icon_image: 1;
        uint8_t enable_table_scroll_anim: 1;
    } flags;
} ESP_Brookesia_RecentsScreenData_t;

#ifdef __cplusplus
}
#endif
