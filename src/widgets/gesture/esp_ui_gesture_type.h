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

typedef enum {
    ESP_UI_GESTURE_DIR_NONE  = 0,
    ESP_UI_GESTURE_DIR_UP    = (1 << 0),
    ESP_UI_GESTURE_DIR_DOWN  = (1 << 1),
    ESP_UI_GESTURE_DIR_LEFT  = (1 << 2),
    ESP_UI_GESTURE_DIR_RIGHT = (1 << 3),
    ESP_UI_GESTURE_DIR_HOR   = (ESP_UI_GESTURE_DIR_LEFT | ESP_UI_GESTURE_DIR_RIGHT),
    ESP_UI_GESTURE_DIR_VER   = (ESP_UI_GESTURE_DIR_UP | ESP_UI_GESTURE_DIR_DOWN),
} ESP_UI_GestureDirection_t;

typedef enum {
    ESP_UI_GESTURE_AREA_CENTER      = 0,
    ESP_UI_GESTURE_AREA_TOP_EDGE    = (1 << 0),
    ESP_UI_GESTURE_AREA_BOTTOM_EDGE = (1 << 1),
    ESP_UI_GESTURE_AREA_LEFT_EDGE   = (1 << 2),
    ESP_UI_GESTURE_AREA_RIGHT_EDGE  = (1 << 3),
} ESP_UI_GestureArea_t;

typedef struct {
    uint8_t detect_period_ms;
    struct {
        uint16_t direction_vertical;
        uint16_t direction_horizon;
        uint8_t direction_angle;
        uint16_t top_edge;
        uint16_t bottom_edge;
        uint16_t left_edge;
        uint16_t right_edge;
        uint16_t duration_short_ms;
        float speed_slow_px_per_ms;
    } threshold;
} ESP_UI_GestureData_t;

typedef struct {
    ESP_UI_GestureDirection_t direction;
    uint8_t start_area;
    uint8_t stop_area;
    int start_x;
    int start_y;
    int stop_x;
    int stop_y;
    uint32_t duration_ms;
    float speed_px_per_ms;
    float distance_px;
    struct {
        uint8_t slow_speed: 1;
        uint8_t short_duration: 1;
    } flags;
} ESP_UI_GestureInfo_t;

#ifdef __cplusplus
}
#endif
