/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Opaque handle for audio DOA instance
 */
typedef void *audio_doa_handle_t;

/**
 * @brief  Callback function type for DOA angle results
 *
 * @param  angle  Detected DOA angle in degrees (0-180)
 * @param  ctx    User-defined context pointer
 */
typedef void (*audio_doa_callback_t)(float angle, void *ctx);

/**
 * @brief  Configuration structure for audio DOA
 *
 *         Currently empty, reserved for future configuration options.
 */
typedef struct {

} audio_doa_config_t;

/**
 * @brief  Create a new audio DOA instance
 *
 * @param  doa_handle  Pointer to store the created DOA handle
 * @param  config      Configuration structure (can be NULL for default configuration)
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid arguments
 *       - ESP_ERR_NO_MEM       Memory allocation failed
 *       - Other                Error code on failure
 */
esp_err_t audio_doa_new(audio_doa_handle_t *doa_handle, audio_doa_config_t *config);

/**
 * @brief  Delete an audio DOA instance
 *
 * @param  doa_handle  DOA handle to delete
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 */
esp_err_t audio_doa_delete(audio_doa_handle_t doa_handle);

/**
 * @brief  Set callback function for DOA angle results
 *
 *         The callback will be called whenever a new DOA angle is calculated.
 *
 * @param  doa_handle  DOA handle
 * @param  cb          Callback function (can be NULL to disable callback)
 * @param  ctx         User-defined context pointer passed to callback
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 */
esp_err_t audio_doa_set_doa_result_callback(audio_doa_handle_t doa_handle, audio_doa_callback_t cb, void *ctx);

/**
 * @brief  Start DOA processing
 *
 *         Starts the DOA processing task and begins accepting audio data.
 *
 * @param  doa_handle  DOA handle
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 *       - ESP_FAIL             DOA handle not initialized
 */
esp_err_t audio_doa_start(audio_doa_handle_t doa_handle);

/**
 * @brief  Stop DOA processing
 *
 *         Stops the DOA processing task.
 *
 * @param  doa_handle  DOA handle
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 */
esp_err_t audio_doa_stop(audio_doa_handle_t doa_handle);

/**
 * @brief  Write audio data to DOA processor
 *
 *         This function should be called with audio data from the microphone array.
 *         The data will be processed to calculate the DOA angle.
 *
 * @param  doa_handle  DOA handle
 * @param  data        Pointer to audio data buffer
 * @param  data_size   Size of audio data in bytes
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid handle or data pointer
 */
esp_err_t audio_doa_data_write(audio_doa_handle_t doa_handle, uint8_t *data, int data_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
