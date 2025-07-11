/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BOT_NUM 2

typedef struct {
    char *public_key;
    char *private_key;
    char *appid;
    char *custom_consumer;
    int bot_num;
    struct bot_config {
        char *bot_id;
        char *voice_id;
        char *bot_name;
        char *bot_description;
    } bot[MAX_BOT_NUM];
} coze_agent_config_t;

esp_err_t coze_agent_config_read(coze_agent_config_t *config);

esp_err_t coze_agent_config_release(coze_agent_config_t *config);

#ifdef __cplusplus
}
#endif
