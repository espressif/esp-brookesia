/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

#ifdef __cplusplus
}
#endif
