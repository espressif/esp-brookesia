/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_log.h"

#include "https_client.h"

#include "openai_signaling.h"


#include <cJSON.h>

#define TAG                   "OPENAI_SIGNALING"

// Prefer to use mini model currently
#define OPENAI_REALTIME_MODEL "gpt-4o-mini-realtime-preview-2024-12-17"
#define OPENAI_REALTIME_URL   "https://api.openai.com/v1/realtime?model=" OPENAI_REALTIME_MODEL


#define SAFE_FREE(p) if (p) {   \
    free(p);                    \
    p = NULL;                   \
}

#define GET_KEY_END(str, key) get_key_end(str, key, sizeof(key) - 1)

typedef struct {
    uint8_t                 *remote_sdp;
    int                      remote_sdp_size;
    char                    *ephemeral_token;
    openai_signaling_cfg_t  cfg;
} openai_signaling_t;

static openai_signaling_t sig;


// static char *get_key_end(char *str, char *key, int len)
// {
//     char *p = strstr(str, key);
//     if (p == NULL) {
//         return NULL;
//     }
//     return p + len;
// }

// static void session_answer(http_resp_t *resp, void *ctx)
// {
// openai_signaling_t *sig = (openai_signaling_t *)ctx;
// char *token = GET_KEY_END((char *)resp->data, "\"client_secret\"");
// if (token == NULL) {
//     return;
// }
// char *secret = GET_KEY_END(token, "\"value\"");
// if (secret == NULL) {
//     return;
// }
// char *s = strchr(secret, '"');
// if (s == NULL) {
//     return;
// }
// s++;
// char *e = strchr(s, '"');
// *e = 0;
// sig.ephemeral_token = strdup(s);
// *e = '"';
// }

static void get_ephemeral_token( char *token, char *voice)
{
    // char content_type[32] = "Content-Type: application/json";
    int len = strlen("Authorization: Bearer ") + strlen(token) + 1;
    char auth[len];
    snprintf(auth, len, "Authorization: Bearer %s", token);
    // char *header[] = {
    //     content_type,
    //     auth,
    //     NULL,
    // };
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", OPENAI_REALTIME_MODEL);
    cJSON *modalities = cJSON_CreateArray();
    cJSON_AddItemToArray(modalities, cJSON_CreateString("text"));
    cJSON_AddItemToArray(modalities, cJSON_CreateString("audio"));
    cJSON_AddItemToObject(root, "modalities", modalities);
    cJSON_AddStringToObject(root, "voice", voice);
    char *json_string = cJSON_Print(root);
    if (json_string) {
        // https_post("https://api.openai.com/v1/realtime/sessions", header, json_string, NULL, session_answer, NULL);
        free(json_string);
    }
    cJSON_Delete(root);
}

esp_err_t openai_signaling_start(openai_signaling_cfg_t *cfg)
{
    sig.cfg = *cfg;
    get_ephemeral_token(cfg->token, cfg->voice ? cfg->voice : "alloy");
    if (sig.ephemeral_token == NULL) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t openai_signaling_stop(void)
{
    return ESP_OK;
}

esp_err_t openai_signaling_send_data(uint8_t *data, size_t size)
{
    return ESP_OK;
}
