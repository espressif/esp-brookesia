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

/** @brief Parameters for the Mosaico BQ27220 fuel gauge. */
typedef struct {
    const char *peripheral_name; /*!< Registered I2C bus name, borrowed until all cleanup completes. */
    uint16_t i2c_addr;           /*!< Seven-bit I2C device address. */
    uint32_t frequency;          /*!< I2C clock frequency in hertz. */
} esp_mosaico_fuel_gauge_config_t;

/** @brief Battery presence and voltage reported by the BQ27220. */
typedef struct {
    bool is_present;     /*!< Whether the gauge reports a connected battery. */
    bool has_voltage_mv; /*!< Whether voltage_mv describes a present battery. */
    uint32_t voltage_mv; /*!< Reported battery voltage in millivolts. */
} bq27220_fuel_gauge_snapshot_t;

/**
 * @brief Reference the I2C bus, attach the gauge, and probe battery status and voltage.
 *
 * Any pending cleanup is retried before initialization. This singleton lifecycle
 * and its snapshot and cleanup operations require external serialization.
 *
 * @param[in] config Gauge parameters; peripheral_name must outlive pending cleanup.
 * @param[out] device_handle Receives the gauge handle, or null on initialization failure
 *                          when both arguments are valid.
 * @return ESP_OK on success, or an argument, allocation, bus, probe, or cleanup error.
 */
esp_err_t esp_mosaico_fuel_gauge_init(const esp_mosaico_fuel_gauge_config_t *config, void **device_handle);

/**
 * @brief Detach the gauge and release its I2C bus reference.
 *
 * Ownership of a valid handle transfers to cleanup even on failure. Do not reuse
 * the handle; use bq27220_fuel_gauge_cleanup() to finish retryable cleanup.
 *
 * @param[in] device_handle Handle returned by esp_mosaico_fuel_gauge_init().
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a null handle, or a cleanup error.
 */
esp_err_t esp_mosaico_fuel_gauge_deinit(void *device_handle);

/**
 * @brief Read battery presence and voltage without changing gauge configuration.
 *
 * @param[in] device_handle Active gauge handle.
 * @param[out] snapshot Receives the snapshot only when both register reads succeed.
 * @return ESP_OK on success, an argument or lifecycle error, or an I2C error.
 */
esp_err_t bq27220_fuel_gauge_get_snapshot(
    void *device_handle, bq27220_fuel_gauge_snapshot_t *snapshot
);

/** @brief Report whether failed initialization or deinitialization still owns resources. */
bool bq27220_fuel_gauge_cleanup_pending(void);

/** @brief Finish pending cleanup; an unknown peripheral release result requires a restart. */
esp_err_t bq27220_fuel_gauge_cleanup(void);

/** @brief Return the most recent cleanup result, including errors hidden by Board Manager. */
esp_err_t bq27220_fuel_gauge_cleanup_error(void);

#ifdef __cplusplus
}
#endif
