/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Parameters for the Mosaico active-low VCC rail and shutdown control. */
typedef struct {
    gpio_num_t vcc_gpio_num;      /*!< Active-low VCC rail enable output. */
    gpio_num_t shutdown_gpio_num; /*!< Open-drain shutdown output, released high during operation. */
    uint32_t ramp_frequency_hz;   /*!< LEDC frequency used while enabling the rail. */
    uint32_t ramp_time_ms;        /*!< Duration of the blocking rail-enable ramp. */
} esp_mosaico_power_config_t;

/**
 * @brief Release shutdown control and enable the VCC rail with a PWM ramp.
 *
 * The ramp temporarily uses low-speed LEDC timer 1 and channel 1. Calls must be
 * serialized with power deinitialization and other users of these LEDC resources.
 *
 * @param[in] config Rail control parameters, needed only during this call.
 * @param[out] device_handle Receives the power handle on success.
 * @return ESP_OK on success, or an argument, allocation, GPIO, or LEDC error.
 */
esp_err_t esp_mosaico_power_init(const esp_mosaico_power_config_t *config, void **device_handle);

/**
 * @brief Turn the VCC rail off and release shutdown control.
 *
 * The handle is consumed even if a GPIO operation fails. Pins retain their final
 * configured levels; this function does not reset them to their default state.
 *
 * @param[in] device_handle Handle returned by esp_mosaico_power_init().
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a null handle, or a GPIO error.
 */
esp_err_t esp_mosaico_power_deinit(void *device_handle);

#ifdef __cplusplus
}
#endif
