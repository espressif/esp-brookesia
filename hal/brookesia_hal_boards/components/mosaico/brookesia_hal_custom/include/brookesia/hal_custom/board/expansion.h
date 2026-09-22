/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Board configuration for the shared Mosaico expansion manager. */
typedef struct {
    const char *peripheral_name;    /*!< I2C peripheral name, valid until manager cleanup completes. */
    int32_t peripheral_count;      /*!< Must be one. */
    int32_t frequency_hz;          /*!< Positive EEPROM I2C frequency. */
    int32_t timeout_ms;            /*!< Positive I2C transaction timeout. */
    int32_t left_address_gpio_num; /*!< Left connector address-selection GPIO. */
    int32_t left_address_level;    /*!< Left connector address-selection level. */
    int32_t left_eeprom_address;   /*!< Left connector EEPROM I2C address. */
    int32_t right_address_gpio_num; /*!< Right connector address-selection GPIO. */
    int32_t right_address_level;   /*!< Right connector address-selection level. */
    int32_t right_eeprom_address;  /*!< Right connector EEPROM I2C address. */
} esp_mosaico_expansion_module_manager_config_t;

/** @brief Board wiring expected by the Mosaico camera lease provider. */
typedef struct {
    int32_t slot;                /*!< Expansion slot containing the camera. */
    int32_t expected_board_type; /*!< Required EEPROM board type. */
    int32_t flash_gpio_num;      /*!< Camera flash GPIO. */
    int32_t flash_off_level;     /*!< GPIO level that keeps the flash off. */
    int32_t settle_time_ms;      /*!< Required camera power settling time. */
} esp_mosaico_camera_slot_claim_config_t;

/**
 * @brief Initialize the board's expansion manager and return its persistent handle.
 *
 * A previously failed cleanup is retried before initialization. The returned
 * handle has board lifetime and must not be freed by the caller.
 *
 * @param[in] config Stable board configuration independent of generated YAML types.
 * @param[out] device_handle Receives the manager handle on success.
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED when expansion is disabled,
 *         or the argument, initialization, or pending-cleanup error.
 */
esp_err_t esp_mosaico_expansion_module_manager_init(
    const esp_mosaico_expansion_module_manager_config_t *config, void **device_handle
);

/**
 * @brief Deinitialize the manager, preserving ownership if cleanup cannot finish.
 * @param[in] device_handle Handle returned by manager initialization.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a null handle,
 *         ESP_ERR_NOT_SUPPORTED when disabled, or the cleanup error.
 */
esp_err_t esp_mosaico_expansion_module_manager_deinit(void *device_handle);

/** @brief Return the most recent board expansion-manager cleanup result. */
esp_err_t esp_mosaico_expansion_cleanup_error(void);

/**
 * @brief Retry expansion-manager cleanup without discarding pending ownership.
 * @return ESP_OK after cleanup, ESP_ERR_NOT_SUPPORTED when disabled, or the cleanup error.
 */
esp_err_t esp_mosaico_expansion_cleanup(void);

/**
 * @brief Create the runtime pin while its Board Manager dependency keeps the manager alive.
 *
 * Pending manager cleanup is recovered without changing Board Manager references
 * inside this callback. The caller must retain the manager dependency until deinit.
 *
 * @param[out] device_handle Receives an owned pin handle; cleared before allocation.
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED when disabled, or an argument,
 *         allocation, or recovery error.
 */
esp_err_t esp_mosaico_expansion_runtime_pin_init(void **device_handle);

/**
 * @brief Free the runtime pin; its caller balances the manager dependency reference.
 * @param[in] device_handle Pin handle returned by initialization.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a null handle, or ESP_ERR_NOT_SUPPORTED when disabled.
 */
esp_err_t esp_mosaico_expansion_runtime_pin_deinit(void *device_handle);

/**
 * @brief Claim a matching camera module using the common expansion runtime.
 * @param[in] config Wiring configuration that must match the shared camera provider.
 * @param[out] device_handle Receives an owned camera lease handle on success.
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED when disabled, ESP_ERR_NOT_FOUND
 *         if no module can be claimed, or an argument/allocation error.
 */
esp_err_t esp_mosaico_camera_slot_claim_init(
    const esp_mosaico_camera_slot_claim_config_t *config, void **device_handle
);

/**
 * @brief Release the camera lease and delete its handle.
 *
 * The runtime relinquishes the lease even if hardware restoration reports an
 * error. That error is logged; an already-empty lease is never retained by the board.
 *
 * @param[in] device_handle Camera lease handle returned by initialization.
 * @return ESP_OK after releasing the handle, ESP_ERR_INVALID_ARG for a null handle,
 *         or ESP_ERR_NOT_SUPPORTED when disabled.
 */
esp_err_t esp_mosaico_camera_slot_claim_deinit(void *device_handle);

#ifdef __cplusplus
}
#endif
