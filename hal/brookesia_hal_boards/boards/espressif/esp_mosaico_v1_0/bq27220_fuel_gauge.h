/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool is_present;
    bool has_voltage_mv;
    uint32_t voltage_mv;
} bq27220_fuel_gauge_snapshot_t;

esp_err_t bq27220_fuel_gauge_get_snapshot(
    void *device_handle, bq27220_fuel_gauge_snapshot_t *snapshot
);

#ifdef __cplusplus
}
#endif
