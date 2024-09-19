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

#define ESP_BROOKESIA_STATUS_BAR_DATA_ICON_IMAGE_NUM_MAX       (6)
#define ESP_BROOKESIA_STATUS_BAR_DATA_AREA_NUM_MAX             (3)

typedef enum {
    ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_UNKNOWN = 0,
    ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_START,
    ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_END,
    ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_CENTER,
    ESP_BROOKESIA_STATUS_BAR_AREA_ALIGN_MAX,
} ESP_Brookesia_StatusBarAreaAlign_t;

typedef struct {
    ESP_Brookesia_StyleSize_t size;
    ESP_Brookesia_StatusBarAreaAlign_t layout_column_align;
    uint16_t layout_column_start_offset;
    uint16_t layout_column_pad;
} ESP_Brookesia_StatusBarAreaData_t;

typedef struct {
    uint8_t image_num;
    ESP_Brookesia_StyleImage_t images[ESP_BROOKESIA_STATUS_BAR_DATA_ICON_IMAGE_NUM_MAX];
} ESP_Brookesia_StatusBarIconImage_t;

typedef struct {
    ESP_Brookesia_StyleSize_t size;
    ESP_Brookesia_StatusBarIconImage_t icon;
} ESP_Brookesia_StatusBarIconData_t;

typedef struct {
    struct {
        ESP_Brookesia_StyleSize_t size;
        ESP_Brookesia_StyleSize_t size_min;
        ESP_Brookesia_StyleSize_t size_max;
        ESP_Brookesia_StyleColor_t background_color;
        ESP_Brookesia_StyleFont_t text_font;
        ESP_Brookesia_StyleColor_t text_color;
    } main;
    struct {
        uint8_t num;
        ESP_Brookesia_StatusBarAreaData_t data[ESP_BROOKESIA_STATUS_BAR_DATA_AREA_NUM_MAX];
    } area;
    ESP_Brookesia_StyleSize_t icon_common_size;
    struct {
        uint8_t area_index;
        ESP_Brookesia_StatusBarIconData_t icon_data;
    } battery;
    struct {
        uint8_t area_index;
        ESP_Brookesia_StatusBarIconData_t icon_data;
    } wifi;
    struct {
        uint8_t area_index;
    } clock;
    struct {
        uint8_t enable_main_size_min: 1;
        uint8_t enable_main_size_max: 1;
        uint32_t enable_battery_icon: 1;
        uint32_t enable_battery_icon_common_size: 1;
        uint32_t enable_battery_label: 1;
        uint32_t enable_wifi_icon: 1;
        uint32_t enable_wifi_icon_common_size: 1;
        uint32_t enable_clock: 1;
    } flags;
} ESP_Brookesia_StatusBarData_t;

typedef enum {
    ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_HIDE = 0,
    ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_SHOW_FIXED,
    ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_SHOW_FLEX,
    ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_MAX,
} ESP_Brookesia_StatusBarVisualMode_t;

#ifdef __cplusplus
}
#endif
