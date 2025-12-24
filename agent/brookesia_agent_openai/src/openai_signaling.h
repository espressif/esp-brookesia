/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *token; /*!< OpenAI token */
    char *voice; /*!< Voice to select */
} openai_signaling_cfg_t;


esp_err_t openai_signaling_start(openai_signaling_cfg_t *cfg);
esp_err_t openai_signaling_stop(void);
esp_err_t openai_signaling_send_data(uint8_t *data, size_t size);

// esp_err_t openai_signaling_send_audio_data(uint8_t *data, size_t size);
// esp_err_t openai_signaling_send_audio_complete(void);
// esp_err_t openai_signaling_send_audio_cancel(void);
// esp_err_t openai_signaling_send_audio_data(uint8_t *data, size_t size);
// esp_err_t openai_signaling_send_audio_complete(void);
// esp_err_t openai_signaling_send_audio_cancel(void);

#ifdef __cplusplus
}
#endif
