/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_gmf_afe.h"
#include "esp_gmf_setup_peripheral.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

enum audio_player_state_e {
    AUDIO_PLAYER_STATE_IDLE,
    AUDIO_PLAYER_STATE_PLAYING,
    AUDIO_PLAYER_STATE_CLOSED,
};

typedef void (*audio_doa_callback_t)(float angle, void *ctx);

/**
 * @brief  Type definition for the audio recorder event callback function
 *
 * @param[in]  event  Pointer to the event data, which can contain information about the recorder's state
 * @param[in]  ctx    User-defined context pointer, passed when registering the callback
 */
typedef void (*recorder_event_callback_t)(void *event, void *ctx);

/**
 * @brief  Initializes the audio manager module.
 *
 * @param[in]  i2c_handle  I2C handle for the audio manager
 * @param[out] play_dev    Pointer to the audio playback device handle
 * @param[out] rec_dev     Pointer to the audio recorder device handle
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_manager_init(esp_gmf_setup_periph_hardware_info *info, void **play_dev, void **rec_dev);

/**
 * @brief  Deinitializes the audio manager component
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_manager_deinit();

/**
 * @brief  Suspend or resume the audio manager
 *
 * @param[in]  suspend  `true` to suspend, `false` to resume
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_manager_suspend(bool suspend);

/**
 * @brief Trigger the AFE to wakeup manually
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_gmf_trigger_wakeup();

/**
 * @brief  Opens the audio recorder and registers an event callback
 *
 * @param[in]  cb   Pointer to the event callback function, which will be invoked on recorder events
 * @param[in]  ctx  User-defined context pointer passed to the callback function
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_recorder_open(recorder_event_callback_t cb, void *ctx);

/**
 * @brief  Closes the audio recorder
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_recorder_close(void);

/**
 * @brief  Reads audio data from the recorder.
 *
 * @param[out]  data       Pointer to the buffer where the audio data will be stored
 * @param[in]   data_size  Size of the buffer to store the audio data.
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_recorder_read_data(uint8_t *data, int data_size);

/**
 * @brief  Opens the audio playback system
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_open();

/**
 * @brief  Closes the audio playback module
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_close(void);

/**
 * @brief  Starts the audio playback operation.
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_run(void);

/**
 * @brief  Stops the ongoing audio playback
 *
 * @return
 *       - ESP_OK  On successfully stopping the audio playback
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_stop(void);

/**
 * @brief  Feeds audio data into the playback system
 *
 * @param[in]  data       Pointer to the audio data to be fed into the playback system
 * @param[in]  data_size  Size of the audio data to be played
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_playback_feed_data(uint8_t *data, int data_size);

/**
 * @brief  Opens the audio prompt system
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_prompt_open(void);

/**
 * @brief  Closes the audio prompt functionality
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_prompt_close(void);

/**
 * @brief  Plays an audio prompt from the specified URL
 *
 * @param[in]  url  URL pointing to the audio prompt file to be played
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_prompt_play(const char *url);

/**
 * @brief  Stops the currently playing audio prompt
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t audio_prompt_stop(void);

esp_err_t audio_prompt_play_with_block(const char *url, int timeout_ms);

esp_gmf_element_handle_t audio_processor_get_afe_handle(void);

esp_err_t audio_prompt_play_mute(bool enable_mute);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
