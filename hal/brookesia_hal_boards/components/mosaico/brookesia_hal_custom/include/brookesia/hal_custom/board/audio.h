/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Configuration for a codec with a dedicated power-enable GPIO. */
typedef struct {
    gpio_num_t codec_gpio_num; /*!< Active-high codec power-enable GPIO. */
    int32_t settle_time_ms;    /*!< Delay after enabling power, in milliseconds. */
} esp_mosaico_audio_power_config_t;

/** @brief Configuration for a codec already powered by the board rail. */
typedef struct {
    int32_t settle_time_ms; /*!< Positive rail-settling delay, in milliseconds. */
} esp_mosaico_audio_ready_config_t;

/**
 * @brief Enable dedicated codec power and wait for the rail to settle.
 * @param config Configuration read during this call.
 * @param device_handle Receives an owned handle; release it with the matching deinit function.
 * @return ESP_OK on success, or an argument, allocation, or GPIO error.
 */
esp_err_t esp_mosaico_audio_power_init(const esp_mosaico_audio_power_config_t *config, void **device_handle);

/**
 * @brief Disable dedicated codec power and release the handle, including on GPIO failure.
 * @param device_handle Handle returned by esp_mosaico_audio_power_init().
 * @return ESP_OK on success, or an argument or GPIO error.
 */
esp_err_t esp_mosaico_audio_power_deinit(void *device_handle);

/**
 * @brief Wait for the board-powered codec without configuring a GPIO.
 * @param config Configuration read during this call; the board rail must already be enabled.
 * @param device_handle Receives a borrowed readiness token for the matching deinit function.
 * @return ESP_OK on success or ESP_ERR_INVALID_ARG for invalid arguments.
 */
esp_err_t esp_mosaico_audio_ready_init(const esp_mosaico_audio_ready_config_t *config, void **device_handle);

/**
 * @brief Validate the readiness token without changing board power.
 * @param device_handle Token returned by esp_mosaico_audio_ready_init().
 * @return ESP_OK for the readiness token or ESP_ERR_INVALID_ARG otherwise.
 */
esp_err_t esp_mosaico_audio_ready_deinit(void *device_handle);

#ifdef __cplusplus
}
#endif
