/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP IO expander: PCA9557
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "esp_io_expander.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a PCA9557 IO expander object
 *
 * @param[in]  i2c_bus    I2C bus handle. Obtained from `i2c_new_master_bus()`
 * @param[in]  dev_addr   I2C device address of chip. Can be `ESP_IO_EXPANDER_I2C_PCA9557_ADDRESS_XXX`
 * @param[out] handle_ret Handle to created IO expander object
 *
 * @return
 *      - ESP_OK: Success, otherwise returns ESP_ERR_xxx
 */
esp_err_t esp_io_expander_new_i2c_pca9557(
        i2c_master_bus_handle_t i2c_bus, uint32_t dev_addr, esp_io_expander_handle_t *handle_ret
);

/**
 * @brief I2C address of the PCA9557
 *
 * The 8-bit address format is as follows:
 *
 *                (Slave Address)
 *     ┌─────────────────┷─────────────────┐
 *  ┌─────┐─────┐─────┐─────┐─────┐─────┐─────┐─────┐
 *  |  0  |  0  |  1  |  1  | A2  | A1  | A0  | R/W |
 *  └─────┘─────┘─────┘─────┘─────┘─────┘─────┘─────┘
 *     └────────┯────────┘     └─────┯──────┘
 *           (Fixed)        (Hareware Selectable)
 *
 * And the 7-bit slave address is the most important data for users.
 * For example, if a chip's A0,A1,A2 are connected to GND, it's 7-bit slave address is 0011000b(0x18).
 * Then users can use `ESP_IO_EXPANDER_I2C_PCA9557_ADDRESS_000` to init it.
 */
#define ESP_IO_EXPANDER_I2C_PCA9557_ADDRESS_000    (0x18)
#define ESP_IO_EXPANDER_I2C_PCA9557_ADDRESS_001    (0x19)
#define ESP_IO_EXPANDER_I2C_PCA9557_ADDRESS_010    (0x1A)
#define ESP_IO_EXPANDER_I2C_PCA9557_ADDRESS_011    (0x1B)
#define ESP_IO_EXPANDER_I2C_PCA9557_ADDRESS_100    (0x1C)
#define ESP_IO_EXPANDER_I2C_PCA9557_ADDRESS_101    (0x1D)
#define ESP_IO_EXPANDER_I2C_PCA9557_ADDRESS_110    (0x1E)
#define ESP_IO_EXPANDER_I2C_PCA9557_ADDRESS_111    (0x1F)

#ifdef __cplusplus
}
#endif
