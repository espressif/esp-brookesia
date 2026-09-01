/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#if BROOKESIA_HAL_ADAPTOR_STORAGE_FILE_SYSTEM_ENABLE_FATFS_NAND
#include "esp_blockdev.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY && BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_IMPL_BQ27220
typedef struct {
    bool is_present;
    bool has_voltage_mv;
    uint32_t voltage_mv;
} bq27220_fuel_gauge_snapshot_t;

esp_err_t bq27220_fuel_gauge_get_snapshot(
    void *device_handle, bq27220_fuel_gauge_snapshot_t *snapshot
) __attribute__((weak));
#endif

#if BROOKESIA_HAL_ADAPTOR_STORAGE_FILE_SYSTEM_ENABLE_FATFS_NAND
esp_err_t fs_nand_get_bdl_handle(
    void *device_handle, esp_blockdev_handle_t *out_handle
) __attribute__((weak));
#endif

#ifdef __cplusplus
}
#endif
