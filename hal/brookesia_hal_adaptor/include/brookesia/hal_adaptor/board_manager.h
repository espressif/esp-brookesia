/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "brookesia/hal_interface/lifecycle.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_board_info esp_board_info_t;

/**
 * @brief Board Manager operations serialized by the shared HAL lifecycle lock.
 *
 * Hold the lifecycle lock around compound init/get transactions. This facade
 * covers Brookesia callers; third-party direct calls require their own coordination.
 */
esp_err_t brookesia_hal_board_manager_init(void);
esp_err_t brookesia_hal_board_manager_get_periph_handle(const char *periph_name, void **periph_handle);
esp_err_t brookesia_hal_board_manager_get_device_handle(const char *dev_name, void **device_handle);
bool brookesia_hal_board_manager_check_name(const char *name);
esp_err_t brookesia_hal_board_manager_get_device_config(const char *dev_name, void **config);
esp_err_t brookesia_hal_board_manager_get_periph_config(const char *periph_name, void **config);
esp_err_t brookesia_hal_board_manager_get_board_info(esp_board_info_t *board_info);
esp_err_t brookesia_hal_board_manager_init_device_by_name(const char *dev_name);
esp_err_t brookesia_hal_board_manager_deinit_device_by_name(const char *dev_name);
esp_err_t brookesia_hal_board_manager_deinit(void);
esp_err_t brookesia_hal_board_periph_init(const char *name);
esp_err_t brookesia_hal_board_periph_get_handle(const char *name, void **periph_handle);
esp_err_t brookesia_hal_board_periph_ref_handle(const char *name, void **periph_handle);
esp_err_t brookesia_hal_board_periph_unref_handle(const char *name);
esp_err_t brookesia_hal_board_periph_get_config(const char *name, void **config);
esp_err_t brookesia_hal_board_periph_deinit(const char *name);
esp_err_t brookesia_hal_board_device_get_handle(const char *name, void **device_handle);
esp_err_t brookesia_hal_board_device_get_i2c_effective_addr(const char *device_name, uint16_t *addr);
esp_err_t brookesia_hal_board_device_show(const char *name);

#ifdef __cplusplus
}
#endif
