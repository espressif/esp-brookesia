/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP LCD touch: CST816S
 */

#pragma once

#include "esp_idf_version.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new CST816S touch driver
 *
 * @note  The I2C communication should be initialized before use this function.
 *
 * @param io LCD panel IO handle, it should be created by `esp_lcd_new_panel_io_i2c()`
 * @param config Touch panel configuration
 * @param tp Touch panel handle
 * @return
 *      - ESP_OK: on success
 */
esp_err_t esp_lcd_touch_new_i2c_cst816s(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *tp);

/**
 * @brief I2C address of the CST816S controller
 *
 */
#define ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS    (0x15)

/**
 * @brief Touch IO configuration structure
 *
 */
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
#define ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG()             \
    {                                                     \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS, \
        .on_color_trans_done = 0,                         \
        .user_ctx = 0,                                    \
        .control_phase_bytes = 1,                         \
        .dc_bit_offset = 0,                               \
        .lcd_cmd_bits = 8,                                \
        .lcd_param_bits = 0,                              \
        .flags =                                          \
        {                                                 \
            .dc_low_on_data = 0,                          \
            .disable_control_phase = 1,                   \
        }                                                 \
    }
#else
#define ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG()             \
    {                                                     \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS, \
        .on_color_trans_done = 0,                         \
        .user_ctx = 0,                                    \
        .control_phase_bytes = 1,                         \
        .dc_bit_offset = 0,                               \
        .lcd_cmd_bits = 8,                                \
        .lcd_param_bits = 0,                              \
        .flags =                                          \
        {                                                 \
            .dc_low_on_data = 0,                          \
            .disable_control_phase = 1,                   \
        },                                                \
        .scl_speed_hz = 100000                            \
    }
#endif

#ifdef __cplusplus
}
#endif
