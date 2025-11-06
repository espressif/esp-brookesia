/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define DOA_ANGLE_CONTROL_UART_TX (GPIO_NUM_6)
#define DOA_ANGLE_CONTROL_UART_RX (GPIO_NUM_5)

/**
 * @brief  Initialize DOA angle control module
 *
 *         Configure and install UART driver for communication with DOA device.
 *         UART configuration: 115200 baud rate, 8 data bits, no parity, 1 stop bit.
 *         This function is weak, and can be overridden by the user.
 *
 * @return
 *       - ESP_OK  Initialization successful
 *       - Other   Error code if initialization failed
 */
esp_err_t doa_angle_control_init(void);

/**
 * @brief  Set DOA angle value
 *
 *         Send control frame via UART to set the target angle of DOA device.
 *         This function is weak, and can be overridden by the user.
 *
 * @param  angle  Angle value (range: 0~65535)
 *
 * @return
 *       - ESP_OK  Setting successful
 *       - Other   Error code if setting failed
 */
esp_err_t doa_angle_control_set_angle(int angle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
