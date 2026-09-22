/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_blockdev.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Parameters for the Mosaico SPI NAND block device. */
typedef struct {
    const char *peripheral_name; /*!< Registered SPI bus name, borrowed until all cleanup completes. */
    gpio_num_t cs_gpio_num;      /*!< NAND chip-select output. */
    gpio_num_t hold_gpio_num;    /*!< NAND HOLD output, held high during operation. */
    gpio_num_t wp_gpio_num;      /*!< NAND write-protect output, held high during operation. */
    int clock_speed_hz;          /*!< SPI clock frequency in hertz. */
    int queue_size;              /*!< Maximum number of queued SPI transactions. */
    uint8_t gc_factor;           /*!< Garbage-collection factor passed to the NAND driver. */
} esp_mosaico_nand_config_t;

/**
 * @brief Reference the SPI bus and initialize the NAND block-device layers.
 *
 * This does not mount or format a filesystem. Any pending cleanup is retried
 * before initialization. This singleton lifecycle and its cleanup operations
 * require external serialization.
 *
 * @param[in] config NAND parameters; peripheral_name must outlive pending cleanup.
 * @param[out] device_handle Receives the NAND handle, or null on initialization failure
 *                          when BDL support is enabled and both arguments are valid.
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED when BDL support is disabled,
 *         or an argument, allocation, GPIO, SPI, NAND, or cleanup error.
 */
esp_err_t esp_mosaico_nand_init(const esp_mosaico_nand_config_t *config, void **device_handle);

/**
 * @brief Synchronize and release NAND layers, the SPI device, bus reference, and protect pins.
 *
 * Ownership of a valid handle transfers to cleanup even on failure. Do not reuse
 * the handle; use fs_nand_cleanup() to finish retryable cleanup. Block-device users
 * must stop using their borrowed handles before deinitialization.
 *
 * @param[in] device_handle Handle returned by esp_mosaico_nand_init().
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a null handle, or a cleanup error.
 */
esp_err_t esp_mosaico_nand_deinit(void *device_handle);

/**
 * @brief Obtain the active NAND block-device handle without transferring ownership.
 *
 * @param[in] device_handle Active NAND handle.
 * @param[out] out_handle Receives a borrowed handle, valid until NAND deinitialization starts.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for null arguments, or ESP_ERR_INVALID_STATE
 *         when cleanup has started or the block-device layers are unavailable.
 */
esp_err_t fs_nand_get_bdl_handle(void *device_handle, esp_blockdev_handle_t *out_handle);

/** @brief Report whether a failed initialization or deinitialization still owns resources. */
bool fs_nand_cleanup_pending(void);

/** @brief Finish pending cleanup; an unknown peripheral release result requires a restart. */
esp_err_t fs_nand_cleanup(void);

/** @brief Return the most recent cleanup result, including errors hidden by Board Manager. */
esp_err_t fs_nand_cleanup_error(void);

#ifdef __cplusplus
}
#endif
