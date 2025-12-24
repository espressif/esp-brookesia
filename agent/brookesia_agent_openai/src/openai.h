/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void  (*audio_data_handler_t)(uint8_t *data, int len, void *ctx);
typedef void (*audio_event_handler_t)(int event, uint8_t *data, void *ctx);

#define OPENAI_DEFAULT_CONNECT_TIMEOUT_MS 10000

#define ESP_PEER_MSG_EVENT   0x1000

typedef struct {
    audio_data_handler_t audio_data_handler;
    audio_event_handler_t audio_event_handler;
    const char *model;
    const char *api_key;
    uint32_t connect_timeout_ms;
    void *ctx;
} openai_config_t;

esp_err_t openai_init(openai_config_t *config);

esp_err_t openai_deinit(void);

esp_err_t openai_start(void);

esp_err_t openai_stop(void);

void openai_send_audio(uint8_t *data, size_t size);

esp_err_t openai_send_text(const char *text);

#ifdef __cplusplus
}
#endif
