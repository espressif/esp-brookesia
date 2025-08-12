/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "led_indicator_ledc.h"


#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Led indicator states
 *
 */
enum {
    BLINK_LOW_POWER = 0,
    BLINK_DEVELOP_MODE,
    BLINK_TOUCH_PRESS_DOWN,
    BLINK_WIFI_CONNECTED,
    BLINK_WIFI_DISCONNECTED,
    BLINK_MAX,
};

/**
 * @brief Led indicator handle
 *
 */
extern led_indicator_handle_t led_indicator_handle;

/**
 * @brief Initialize led indicator
 *
 * @return bool true if initialized successfully, false if failed
 */
bool led_indicator_init(void);

/**
 * @brief Led indicator register wifi event
 *
 * @return true if registered successfully, false if failed
 */
bool led_indicator_register_wifi_event(void);

#if defined(__cplusplus)
}
#endif
