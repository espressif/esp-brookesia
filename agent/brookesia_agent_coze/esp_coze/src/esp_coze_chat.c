/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_err.h"
#include "mbedtls/base64.h"
#include "cJSON.h"
#include "esp_coze_utils.h"
#include "esp_coze_chat.h"

#define ESP_COZE_CHAT_LOCK(chat)   xSemaphoreTake(chat->lock, portMAX_DELAY)
#define ESP_COZE_CHAT_UNLOCK(chat) xSemaphoreGive(chat->lock)

#define EVENT_ID_LENGTH            (40)
#define MAX_URL_LENGTH             (256)
#define MAX_TOKEN_LENGTH           (256)
#define MAX_AUDIO_SIZE             (64 * 1024)
#define WS_CONNECTED_BIT           (1 << 0)
#define DEFALUT_AUDIO_DATA_TIMEOUT (20000)  // 20s

static const char *TAG = "ESP_COZE_CHAT";

typedef struct coze_parameter_list {
    esp_coze_parameters_kv_t *parameters;
    int                      cnt;
} coze_parameter_list_t;

typedef struct {
    char       *function_name;
    esp_err_t (*function)(esp_coze_chat_handle_t chat, char *json_data);
} esp_chat_function_map_t;

typedef struct {
    esp_websocket_client_handle_t client;
    esp_coze_chat_config_t        config;
    char                         *conversation_id;
    char                         *event_id;
    char                         *data_ptr;
    bool                         conversation_id_set;
    coze_parameter_list_t        chat_parameters;
    SemaphoreHandle_t            lock;
    EventGroupHandle_t           event_group;
} coze_chat_t;

static esp_err_t process_conversation_audio_delta(esp_coze_chat_handle_t chat, char *json_data);
static esp_err_t process_conversation_message_delta(esp_coze_chat_handle_t chat, char *json_data);
static esp_err_t process_conversation_chat_completed(esp_coze_chat_handle_t chat, char *json_data);
static esp_err_t process_conversation_speech_started(esp_coze_chat_handle_t chat, char *json_string);
static esp_err_t process_conversation_speech_stopped(esp_coze_chat_handle_t chat, char *json_string);
static esp_err_t process_conversation_chat_update(esp_coze_chat_handle_t chat, char *json_string);
static esp_err_t process_conversation_error(esp_coze_chat_handle_t chat, char *json_string);

static const esp_chat_function_map_t s_function_map[] = {
    { .function_name = "conversation.audio.delta", .function = process_conversation_audio_delta },
    { .function_name = "conversation.message.delta", .function = process_conversation_message_delta },
    { .function_name = "conversation.chat.completed", .function = process_conversation_chat_completed },
    { .function_name = "input_audio_buffer.speech_started", .function = process_conversation_speech_started },
    { .function_name = "input_audio_buffer.speech_stopped", .function = process_conversation_speech_stopped },
    { .function_name = "chat.created", .function = process_conversation_chat_update },
    { .function_name = "error", .function = process_conversation_error },
};

static void *coze_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    void *data = NULL;
#if CONFIG_SPIRAM_BOOT_INIT
    data = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    data = malloc(size);
#endif
    return data;
}

static char *coze_strdup(const char *str)
{
    if (!str) {
        return NULL;
    }

    size_t size = strlen(str) + 1;
#if CONFIG_SPIRAM_BOOT_INIT
    char *copy = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    char *copy = malloc(size);
#endif
    if (copy) {
        strcpy(copy, str);
    }
    return copy;
}

static esp_err_t gen_random_event_id(coze_chat_t *chat)
{
    if (!chat) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!chat->event_id) {
        chat->event_id = coze_malloc(EVENT_ID_LENGTH);
        if (!chat->event_id) {
            return ESP_ERR_NO_MEM;
        }
    }

    snprintf(chat->event_id, EVENT_ID_LENGTH - 1, "%08x-%04x-%04x-%04x-%04x%08x",
             (int)esp_random() & 0x1FFFFFFF,
             (int)esp_random() & 0xFFFF,
             (int)esp_random() & 0xFFFF,
             (int)esp_random() & 0xFFFF,
             (int)esp_random() & 0xFFFF,
             (int)esp_random() & 0xFFFFFFFF);
    return ESP_OK;
}

static void _chat_event_dispatch(coze_chat_t *chat, char *out_data, esp_coze_chat_event_t event)
{
    if (chat && chat->config.event_callback) {
        chat->config.event_callback(event, out_data, chat->config.event_callback_ctx);
    }
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static esp_err_t safe_json_parse_and_cleanup(cJSON **json, const char *json_data)
{
    if (!json_data) {
        return ESP_ERR_INVALID_ARG;
    }

    *json = cJSON_Parse(json_data);
    if (!*json) {
        ESP_LOGE(TAG, "JSON parse failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void safe_json_delete(cJSON **json)
{
    if (json && *json) {
        cJSON_Delete(*json);
        *json = NULL;
    }
}

static esp_err_t safe_memory_free(void **ptr)
{
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
    return ESP_OK;
}

static esp_err_t websocket_send_with_retry(esp_websocket_client_handle_t client,
        const char *data,
        int len,
        int max_retries,
        int retry_delay_ms,
        const char *operation_name)
{
    if (!client || !data || len <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for websocket send");
        return ESP_ERR_INVALID_ARG;
    }

    if (max_retries < 0 || retry_delay_ms < 0) {
        ESP_LOGE(TAG, "Invalid retry parameters: max_retries=%d, retry_delay_ms=%d",
                 max_retries, retry_delay_ms);
        return ESP_ERR_INVALID_ARG;
    }

    int retry_count = 0;
    esp_err_t ret = ESP_FAIL;

    while (retry_count < max_retries) {
        int send_result = esp_websocket_client_send_text(client, data, len, portMAX_DELAY);
        if (send_result != -1) {
            if (operation_name) {
                ESP_LOGD(TAG, "%s sent successfully on attempt %d", operation_name, retry_count + 1);
            } else {
                ESP_LOGD(TAG, "WebSocket data sent successfully on attempt %d", retry_count + 1);
            }
            ret = ESP_OK;
            break;
        } else {
            retry_count++;
            if (operation_name) {
                ESP_LOGW(TAG, "%s send failed on attempt %d/%d", operation_name, retry_count, max_retries);
            } else {
                ESP_LOGW(TAG, "WebSocket send failed on attempt %d/%d", retry_count, max_retries);
            }

            if (retry_count < max_retries) {
                vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
            }
        }
    }

    if (ret != ESP_OK) {
        if (operation_name) {
            ESP_LOGE(TAG, "%s send failed after %d attempts", operation_name, max_retries);
        } else {
            ESP_LOGE(TAG, "WebSocket send failed after %d attempts", max_retries);
        }
    }

    return ret;
}

static esp_err_t decode_base64_audio_data(const char *base64_data, unsigned char **decoded_data, size_t *decoded_len)
{
    if (!base64_data || !decoded_data || !decoded_len) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t base64_len = strlen(base64_data);
    if (base64_len == 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t output_len = 0;
    int ret = mbedtls_base64_decode(NULL, 0, &output_len,
                                    (const unsigned char *)base64_data, base64_len);
    if (output_len > 65535) {
        ESP_LOGE(TAG, "Base64 decoded length is too long: %d", output_len);
        return ESP_ERR_INVALID_SIZE;
    }
    if (output_len == 0) {
        ESP_LOGE(TAG, "Base64 decoded length is zero");
        return ESP_ERR_INVALID_SIZE;
    }

    *decoded_data = coze_malloc(output_len);
    if (!*decoded_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for decoded audio data");
        return ESP_ERR_NO_MEM;
    }

    ret = mbedtls_base64_decode(*decoded_data, output_len, &output_len,
                                (const unsigned char *)base64_data, base64_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_base64_decode failed: %d", ret);
        safe_memory_free((void **)decoded_data);
        return ESP_FAIL;
    }

    *decoded_len = output_len;
    return ESP_OK;
}

static esp_err_t process_conversation_audio_delta(esp_coze_chat_handle_t chat, char *json_data)
{
    if (!chat || !json_data) {
        return ESP_ERR_INVALID_ARG;
    }

    coze_chat_t *chat_obj = (coze_chat_t *)chat;
    cJSON *json = NULL;
    esp_err_t ret = safe_json_parse_and_cleanup(&json, json_data);
    if (ret != ESP_OK) {
        return ret;
    }

    cJSON *id = cJSON_GetObjectItem(json, "id");
    if (cJSON_IsString(id)) {
        ESP_LOGD(TAG, "id: %s", id->valuestring);
    }

    cJSON *event_type = cJSON_GetObjectItem(json, "event_type");
    if (cJSON_IsString(event_type)) {
        ESP_LOGD(TAG, "event_type: %s", event_type->valuestring);
    }

    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (cJSON_IsObject(data)) {
        cJSON *data_id = cJSON_GetObjectItem(data, "id");
        cJSON *role = cJSON_GetObjectItem(data, "role");
        cJSON *content = cJSON_GetObjectItem(data, "content");

        if (cJSON_IsString(data_id)) {
            ESP_LOGD(TAG, "data.id: %s", data_id->valuestring);
        }
        if (cJSON_IsString(role)) {
            ESP_LOGD(TAG, "data.role: %s", role->valuestring);
        }
        if (cJSON_IsString(content)) {
            unsigned char *decoded_audio = NULL;
            size_t decoded_len = 0;
            ret = decode_base64_audio_data(content->valuestring, &decoded_audio, &decoded_len);
            if (ret == ESP_OK) {
                if (chat_obj->config.audio_callback) {
                    chat_obj->config.audio_callback((char *)decoded_audio, decoded_len,
                                                    chat_obj->config.audio_callback_ctx);
                }
                safe_memory_free((void **)&decoded_audio);
            } else {
                ESP_LOGE(TAG, "Failed to decode audio data: %s", esp_err_to_name(ret));
            }
        }
    }

    cJSON *detail = cJSON_GetObjectItem(json, "detail");
    if (cJSON_IsObject(detail)) {
        cJSON *logid = cJSON_GetObjectItem(detail, "logid");
        if (cJSON_IsString(logid)) {
            ESP_LOGD(TAG, "detail.logid: %s", logid->valuestring);
        }
    }

    safe_json_delete(&json);
    return ESP_OK;
}

static esp_err_t process_conversation_message_delta(esp_coze_chat_handle_t chat, char *json_data)
{
    if (!chat || !json_data) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = NULL;
    esp_err_t ret = safe_json_parse_and_cleanup(&root, json_data);
    if (ret != ESP_OK) {
        return ret;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data) {
        cJSON *content = cJSON_GetObjectItem(data, "content");
        if (content && cJSON_IsString(content)) {
            ESP_LOGD(TAG, "content: %s", content->valuestring);
            _chat_event_dispatch((coze_chat_t *)chat, (char *)content->valuestring, ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT);
        } else {
            ESP_LOGE(TAG, "Content field not found or not a string");
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Data field not found");
        ret = ESP_FAIL;
    }

    safe_json_delete(&root);
    return ret;
}

static esp_err_t process_conversation_chat_completed(esp_coze_chat_handle_t chat, char *json_data)
{
    _chat_event_dispatch((coze_chat_t *)chat, NULL, ESP_COZE_CHAT_EVENT_CHAT_COMPLETED);
    return ESP_OK;
}

static esp_err_t process_conversation_speech_stopped(esp_coze_chat_handle_t chat_hd, char *json_string)
{
    ESP_LOGI(TAG, "speech stopped");
    _chat_event_dispatch((coze_chat_t *)chat_hd, NULL, ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STOPED);
    return ESP_OK;
}

static esp_err_t process_conversation_speech_started(esp_coze_chat_handle_t chat_hd, char *json_string)
{
    ESP_LOGI(TAG, "speech started");
    _chat_event_dispatch((coze_chat_t *)chat_hd, NULL, ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STARTED);
    return ESP_OK;
}

static esp_err_t process_conversation_error(esp_coze_chat_handle_t chat_hd, char *json_string)
{
    ESP_LOGI(TAG, "coze websocket error reason: %s", json_string);
    _chat_event_dispatch((coze_chat_t *)chat_hd, json_string, ESP_COZE_CHAT_EVENT_CHAT_ERROR);
    return ESP_OK;
}

static esp_err_t process_conversation_chat_update(esp_coze_chat_handle_t chat_hd, char *json_string)
{
    if (!json_string) {
        ESP_LOGE(TAG, "JSON string is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON parse error");
        return ESP_FAIL;
    }
    cJSON *id = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(id)) {
        ESP_LOGD(TAG, "ID: %s", id->valuestring);
    }

    cJSON *event_type = cJSON_GetObjectItem(root, "event_type");
    if (cJSON_IsString(event_type)) {
        ESP_LOGD(TAG, "Event Type: %s\n", event_type->valuestring);
    }

    cJSON *detail = cJSON_GetObjectItem(root, "detail");
    if (cJSON_IsObject(detail)) {
        cJSON *logid = cJSON_GetObjectItem(detail, "logid");
        if (cJSON_IsString(logid)) {
            ESP_LOGI(TAG, "Log ID: %s", logid->valuestring);
        }
    }
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t on_message(esp_coze_chat_handle_t chat_hd, char *data)
{
    cJSON *json = cJSON_Parse(data);
    if (json == NULL) {
        return ESP_FAIL;
    }
    cJSON *event_type_item = cJSON_GetObjectItem(json, "event_type");
    if (cJSON_IsString(event_type_item) && (event_type_item->valuestring != NULL)) {
        for (int i = 0; i < sizeof(s_function_map) / sizeof(s_function_map[0]); i++) {
            if (strcmp(s_function_map[i].function_name, event_type_item->valuestring) == 0) {
                ESP_LOGD(TAG, "Event type:  %s", s_function_map[i].function_name);
                s_function_map[i].function(chat_hd, data);
                cJSON_Delete(json);
                return ESP_OK;
            }
        }
        coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
        chat_obj->config.event_callback(ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA, data, chat_obj->config.event_callback_ctx);
    }
    cJSON_Delete(json);
    return ESP_OK;
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    coze_chat_t *chat_obj = (coze_chat_t *)handler_args;
    if (chat_obj->config.ws_event_callback) {
        esp_coze_ws_event_t event = {
            .handle = (void *)chat_obj->client,
            .event_id = event_id,
        };
        chat_obj->config.ws_event_callback(&event);
    }
    switch (event_id) {
    case WEBSOCKET_EVENT_BEGIN:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN");
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        xEventGroupSetBits(chat_obj->event_group, WS_CONNECTED_BIT);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGD(TAG, "on data, opcode=%d, total payload=%d, data_len=%d, offset=%d", data->op_code, data->payload_len, data->data_len, data->payload_offset);
        if (data->op_code == 0x2) { // Opcode 0x2 indicates binary data
            ESP_LOG_BUFFER_HEX("Received binary data", data->data_ptr, data->data_len);
            ESP_LOGD(TAG, "Received binary data, len=%d, data->data_ptr: %s", data->data_len, data->data_ptr);
        } else if (data->op_code == 0x08 && data->data_len == 2) {
            ESP_LOGD(TAG, "Received closed message with code=%d", 256 * data->data_ptr[0] + data->data_ptr[1]);
        } else if (data->op_code == 0x01) {
            if (data->payload_offset == 0) {
                if (chat_obj->data_ptr) {
                    free(chat_obj->data_ptr);
                }
                chat_obj->data_ptr = coze_malloc(data->payload_len + 1);
                if (chat_obj->data_ptr == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory for data_ptr");
                    return;
                }
            }
            memcpy(chat_obj->data_ptr + data->payload_offset, data->data_ptr, data->data_len);
            if (data->payload_len == data->payload_offset + data->data_len) {
                on_message(handler_args, (char *)chat_obj->data_ptr);
                free(chat_obj->data_ptr);
                chat_obj->data_ptr = NULL;
            }
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
        log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_FINISH");
        break;
    }
}

esp_err_t esp_coze_chat_init(esp_coze_chat_config_t *config, esp_coze_chat_handle_t *chat_hd)
{
    if (!config || !chat_hd) {
        ESP_LOGE(TAG, "Invalid parameters: config=%p, chat_hd=%p", config, chat_hd);
        return ESP_ERR_INVALID_ARG;
    }

    if (!config->bot_id || !config->access_token || !config->voice_id) {
        ESP_LOGE(TAG, "Missing required config parameters");
        return ESP_ERR_INVALID_ARG;
    }

    coze_chat_t *chat_obj = coze_malloc(sizeof(coze_chat_t));
    if (!chat_obj) {
        ESP_LOGE(TAG, "Failed to allocate chat object");
        return ESP_ERR_NO_MEM;
    }

    memset(chat_obj, 0, sizeof(coze_chat_t));
    memcpy(&chat_obj->config, config, sizeof(esp_coze_chat_config_t));

    chat_obj->config.voice_id = coze_strdup(config->voice_id);
    if (!chat_obj->config.voice_id) {
        ESP_LOGE(TAG, "Failed to allocate memory for voice_id");
        safe_memory_free((void **)&chat_obj);
        return ESP_ERR_NO_MEM;
    }

    char wss_url[MAX_URL_LENGTH] = { 0 };
    char token[MAX_TOKEN_LENGTH] = { 0 };

    int r = snprintf(wss_url, sizeof(wss_url), "%s?bot_id=%s", config->ws_base_url, config->bot_id);
    if (r >= sizeof(wss_url) || r < 0) {
        ESP_LOGE(TAG, "URL too long or snprintf failed");
        safe_memory_free((void **)&chat_obj->config.voice_id);
        safe_memory_free((void **)&chat_obj);
        return ESP_ERR_INVALID_SIZE;
    }

    r = snprintf(token, sizeof(token), "Bearer %s", config->access_token);
    if (r >= sizeof(token) || r < 0) {
        ESP_LOGE(TAG, "Token too long or snprintf failed");
        safe_memory_free((void **)&chat_obj->config.voice_id);
        safe_memory_free((void **)&chat_obj);
        return ESP_ERR_INVALID_SIZE;
    }
    ESP_LOGI(TAG, "token: %s", token);

    esp_websocket_client_config_t websocket_cfg = { 0 };
    websocket_cfg.reconnect_timeout_ms = 1000;
    websocket_cfg.network_timeout_ms = 5000;
    websocket_cfg.buffer_size = config->websocket_buffer_size;
    websocket_cfg.uri = wss_url;
    websocket_cfg.task_prio = 12;

    chat_obj->client = esp_websocket_client_init(&websocket_cfg);
    if (!chat_obj->client) {
        ESP_LOGE(TAG, "Failed to create websocket client");
        safe_memory_free((void **)&chat_obj->config.voice_id);
        safe_memory_free((void **)&chat_obj);
        return ESP_FAIL;
    }

    esp_websocket_register_events(chat_obj->client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)chat_obj);
    esp_websocket_client_append_header(chat_obj->client, "Authorization", token);

    chat_obj->lock = xSemaphoreCreateMutex();
    if (!chat_obj->lock) {
        ESP_LOGE(TAG, "Failed to create lock");
        esp_websocket_client_destroy(chat_obj->client);
        safe_memory_free((void **)&chat_obj->config.voice_id);
        safe_memory_free((void **)&chat_obj);
        return ESP_FAIL;
    }

    chat_obj->event_group = xEventGroupCreate();
    if (!chat_obj->event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        vSemaphoreDelete(chat_obj->lock);
        esp_websocket_client_destroy(chat_obj->client);
        safe_memory_free((void **)&chat_obj->config.voice_id);
        safe_memory_free((void **)&chat_obj);
        return ESP_FAIL;
    }

    *chat_hd = chat_obj;
    ESP_LOGI(TAG, "Chat object initialized successfully");
    return ESP_OK;
}

esp_err_t esp_coze_chat_deinit(esp_coze_chat_handle_t chat_hd)
{
    if (chat_hd == NULL) {
        ESP_LOGE(TAG, "chat is NULL");
        return ESP_FAIL;
    }
    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
    if (chat_obj->client) {
        esp_websocket_client_destroy(chat_obj->client);
    }
    if (chat_obj->conversation_id) {
        free(chat_obj->conversation_id);
        chat_obj->conversation_id = NULL;
    }
    if (chat_obj->lock) {
        vSemaphoreDelete(chat_obj->lock);
    }
    if (chat_obj->event_group) {
        vEventGroupDelete(chat_obj->event_group);
    }
    if (chat_obj->event_id) {
        free(chat_obj->event_id);
    }
    if (chat_obj->data_ptr) {
        free(chat_obj->data_ptr);
    }
    if (chat_obj->config.voice_id) {
        free(chat_obj->config.voice_id);
    }
    if (chat_obj->chat_parameters.parameters) {
        for (int i = 0; i < chat_obj->chat_parameters.cnt; i++) {
            if (chat_obj->chat_parameters.parameters[i].key) {
                free(chat_obj->chat_parameters.parameters[i].key);
            }
            if (chat_obj->chat_parameters.parameters[i].value) {
                free(chat_obj->chat_parameters.parameters[i].value);
            }
        }
        free(chat_obj->chat_parameters.parameters);
    }
    free(chat_obj);
    return ESP_OK;
}

esp_err_t esp_coze_chat_start(esp_coze_chat_handle_t chat_hd)
{
    if (chat_hd == NULL) {
        ESP_LOGE(TAG, "chat is NULL");
        return ESP_FAIL;
    }
    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
    esp_websocket_client_start(chat_obj->client);
    EventBits_t uxBits = xEventGroupWaitBits(chat_obj->event_group, WS_CONNECTED_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(chat_obj->config.websocket_connect_timeout));
    if (uxBits & WS_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WS connected");
    } else {
        ESP_LOGE(TAG, "websocket connect timeout");
        return ESP_ERR_TIMEOUT;
    }

    if (chat_obj->conversation_id_set == false) {
        char token[128] = { 0 };
        snprintf(token, sizeof(token), "Bearer %s", chat_obj->config.access_token);
        esp_websocket_client_append_header(chat_obj->client, "Authorization", token);
        // Note: lock and event_group are already created in esp_coze_chat_init
    }

    ESP_LOGI(TAG, "Coze chat start updata chat");
    esp_coze_chat_update_chat(chat_obj);
    return ESP_OK;
}

esp_err_t esp_coze_chat_stop(esp_coze_chat_handle_t chat_hd)
{
    if (chat_hd == NULL) {
        ESP_LOGE(TAG, "chat is NULL");
        return ESP_FAIL;
    }
    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
    if (chat_obj->client) {
        esp_websocket_client_stop(chat_obj->client);
    }
    return ESP_OK;
}

esp_err_t esp_coze_set_chat_config_voice_id(esp_coze_chat_handle_t chat_hd, const char *voice_id)
{
    if (!chat_hd || !voice_id) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
    if (chat_obj->config.voice_id) {
        safe_memory_free((void **)&chat_obj->config.voice_id);
    }
    chat_obj->config.voice_id = coze_strdup(voice_id);
    if (chat_obj->config.voice_id == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for voice_id");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t esp_coze_set_chat_config_parameters(esp_coze_chat_handle_t chat_hd, esp_coze_parameters_kv_t *key_val)
{
    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
    if (key_val == NULL) {
        ESP_LOGE(TAG, "key_val is NULL");
        return ESP_FAIL;
    }
    if (chat_obj->chat_parameters.cnt > 0) {
        for (int i = 0; i < chat_obj->chat_parameters.cnt; i++) {
            if (chat_obj->chat_parameters.parameters[i].key) {
                free(chat_obj->chat_parameters.parameters[i].key);
            }
            if (chat_obj->chat_parameters.parameters[i].value) {
                free(chat_obj->chat_parameters.parameters[i].value);
            }
        }
        free(chat_obj->chat_parameters.parameters);
    }
    esp_coze_parameters_kv_t *_tmp = key_val;
    while (_tmp->key != NULL) {
        _tmp++;
        chat_obj->chat_parameters.cnt++;
    }
    chat_obj->chat_parameters.parameters = calloc(1, sizeof(esp_coze_parameters_kv_t) * chat_obj->chat_parameters.cnt);
    if (chat_obj->chat_parameters.parameters == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for chat_parameters\n");
        return ESP_FAIL;
    }
    _tmp = key_val;
    int i = 0;
    while (_tmp->key != NULL) {
        chat_obj->chat_parameters.parameters[i].key = calloc(1, strlen(_tmp->key) + 1);
        if (chat_obj->chat_parameters.parameters[i].key == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for chat_parameters\n");
            goto _exit;
        }
        strcpy(chat_obj->chat_parameters.parameters[i].key, _tmp->key);
        chat_obj->chat_parameters.parameters[i].value = calloc(1, strlen(_tmp->value) + 1);
        if (chat_obj->chat_parameters.parameters[i].value == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for chat_parameters\n");
            // Free the already allocated key before returning
            free(chat_obj->chat_parameters.parameters[i].key);
            chat_obj->chat_parameters.parameters[i].key = NULL;
            goto _exit;
        }
        strcpy(chat_obj->chat_parameters.parameters[i].value, _tmp->value);
        i++;
        _tmp++;
    }
    return ESP_OK;

_exit:
    for (int i = 0; i < chat_obj->chat_parameters.cnt; i++) {
        if (chat_obj->chat_parameters.parameters[i].key) {
            free(chat_obj->chat_parameters.parameters[i].key);
        }
        if (chat_obj->chat_parameters.parameters[i].value) {
            free(chat_obj->chat_parameters.parameters[i].value);
        }
    }
    free(chat_obj->chat_parameters.parameters);
    return ESP_FAIL;
}

esp_err_t esp_coze_set_chat_config_conversation_id(esp_coze_chat_handle_t chat_hd, char *conversation_id)
{
    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
    if (chat_obj->conversation_id) {
        free(chat_obj->conversation_id);
    }
    chat_obj->conversation_id = calloc(1, strlen(conversation_id) + 1);
    if (chat_obj->conversation_id == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for conversation_id\n");
        return ESP_FAIL;
    }
    strcpy(chat_obj->conversation_id, conversation_id);
    chat_obj->conversation_id_set = true;
    return ESP_OK;
}

esp_err_t esp_coze_get_chat_config_conversation_id(esp_coze_chat_handle_t chat_hd, char **conversation_id)
{
    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
    if (chat_obj->conversation_id == NULL) {
        ESP_LOGE(TAG, "Conversation ID is not set\n");
        return ESP_FAIL;
    }
    *conversation_id = chat_obj->conversation_id;
    return ESP_OK;
}

esp_err_t esp_coze_chat_send_audio_data(esp_coze_chat_handle_t chat_hd, char *data, int len)
{
    if (!chat_hd || !data || len <= 0) {
        ESP_LOGE(TAG, "Invalid parameters: chat_hd=%p, data=%p, len=%d", chat_hd, data, len);
        return ESP_ERR_INVALID_ARG;
    }

    if (len > MAX_AUDIO_SIZE) {
        ESP_LOGE(TAG, "Audio data too large: %d bytes (max: %d)", len, MAX_AUDIO_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;

    if (!esp_websocket_client_is_connected(chat_obj->client)) {
        ESP_LOGE(TAG, "WebSocket not connected");
        return ESP_FAIL;
    }

    size_t base64_len = ((len + 2) / 3) * 4 + 1;
    unsigned char *base64_data = coze_malloc(base64_len);
    if (!base64_data) {
        ESP_LOGE(TAG, "Failed to allocate base64 buffer");
        return ESP_ERR_NO_MEM;
    }

    size_t encoded_len = 0;
    int result = mbedtls_base64_encode(base64_data, base64_len, &encoded_len,
                                       (const unsigned char *)data, len);
    if (result != 0) {
        ESP_LOGE(TAG, "Base64 encode failed: %d", result);
        safe_memory_free((void **)&base64_data);
        return ESP_FAIL;
    }

    cJSON *json_root = cJSON_CreateObject();
    if (!json_root) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        safe_memory_free((void **)&base64_data);
        return ESP_ERR_NO_MEM;
    }

    if (gen_random_event_id(chat_obj) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate event ID");
        cJSON_Delete(json_root);
        safe_memory_free((void **)&base64_data);
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(json_root, "id", chat_obj->event_id);
    cJSON_AddStringToObject(json_root, "event_type", "input_audio_buffer.append");

    cJSON *data_obj = cJSON_CreateObject();
    if (!data_obj) {
        ESP_LOGE(TAG, "Failed to create data object");
        cJSON_Delete(json_root);
        safe_memory_free((void **)&base64_data);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(data_obj, "delta", (char *)base64_data);
    cJSON_AddItemToObject(json_root, "data", data_obj);

    char *json_string = cJSON_PrintUnformatted(json_root);
    if (!json_string) {
        ESP_LOGE(TAG, "Failed to print JSON");
        cJSON_Delete(json_root);
        safe_memory_free((void **)&base64_data);
        return ESP_ERR_NO_MEM;
    }

    // printf("json_string: %s\n", json_string);

    int send_result = esp_websocket_client_send_text(chat_obj->client, json_string,
                      strlen(json_string), DEFALUT_AUDIO_DATA_TIMEOUT);

    esp_err_t ret = ESP_OK;
    if (send_result == -1) {
        ESP_LOGE(TAG, "WebSocket send failed");
        ret = ESP_FAIL;
    } else {
        ESP_LOGD(TAG, "Audio data sent successfully, length: %d", len);
    }

    safe_memory_free((void **)&json_string);
    cJSON_Delete(json_root);
    safe_memory_free((void **)&base64_data);

    return ret;
}

esp_err_t esp_coze_chat_audio_data_clearup(esp_coze_chat_handle_t chat_hd)
{
    esp_err_t ret = ESP_OK;
    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
    ESP_COZE_CHAT_LOCK(chat_obj);
    cJSON *json_obj = cJSON_CreateObject();
    gen_random_event_id(chat_obj);
    cJSON_AddStringToObject(json_obj, "id", chat_obj->event_id);
    cJSON_AddStringToObject(json_obj, "event_type", "input_audio_buffer.clear");
    char *json_str = cJSON_Print(json_obj);
    ESP_LOGI(TAG, "Audio clear: %s", json_str);
    ret = websocket_send_with_retry(chat_obj->client, json_str, strlen(json_str),
                                    3, 50, "Audio clear");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send audio clear failed: %s", esp_err_to_name(ret));
    }
    cJSON_Delete(json_obj);
    free(json_str);
    ESP_COZE_CHAT_UNLOCK(chat_obj);
    return ret;
}

esp_err_t esp_coze_chat_send_audio_complete(esp_coze_chat_handle_t chat_hd)
{
    esp_err_t ret = ESP_OK;
    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
    ESP_COZE_CHAT_LOCK(chat_obj);
    cJSON *json_obj = cJSON_CreateObject();
    gen_random_event_id(chat_obj);
    cJSON_AddStringToObject(json_obj, "id", chat_obj->event_id);
    cJSON_AddStringToObject(json_obj, "event_type", "input_audio_buffer.complete");
    cJSON *data_obj = cJSON_CreateObject();
    cJSON_AddItemToObject(json_obj, "data", data_obj);
    char *json_str = cJSON_Print(json_obj);
    ESP_LOGI(TAG, "Audio complete: %s", json_str);
    ret = websocket_send_with_retry(chat_obj->client, json_str, strlen(json_str),
                                    3, 50, "Audio complete");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send audio complete failed: %s", esp_err_to_name(ret));
    }
    cJSON_Delete(json_obj);
    free(json_str);
    ESP_COZE_CHAT_UNLOCK(chat_obj);
    return ret;
}

esp_err_t esp_coze_chat_send_audio_cancel(esp_coze_chat_handle_t chat_hd)
{
    if (!chat_hd) {
        ESP_LOGE(TAG, "Invalid chat handle");
        return ESP_ERR_INVALID_ARG;
    }

    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_ERR_NO_MEM;
    }

    if (gen_random_event_id(chat_obj) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate event ID");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(root, "id", chat_obj->event_id);
    cJSON_AddStringToObject(root, "event_type", "conversation.chat.cancel");

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON");
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Terminal %s", json_string);

    esp_err_t ret = websocket_send_with_retry(chat_obj->client, json_string, strlen(json_string),
                    5, 100, "Audio cancel");

    safe_memory_free((void **)&json_string);
    cJSON_Delete(root);
    return ret;
}

esp_err_t esp_coze_chat_update_chat(esp_coze_chat_handle_t chat_hd)
{
    esp_err_t ret = ESP_OK;
    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
    ESP_COZE_CHAT_LOCK(chat_obj);
    cJSON *root = cJSON_CreateObject();
    gen_random_event_id(chat_obj);
    cJSON_AddStringToObject(root, "id", chat_obj->event_id);
    cJSON_AddStringToObject(root, "event_type", "chat.update");

    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "data", data);

    cJSON *chat_config = cJSON_CreateObject();
    cJSON_AddBoolToObject(chat_config, "auto_save_history", 1);
    cJSON_AddStringToObject(chat_config, "conversation_id", chat_obj->conversation_id);
    cJSON_AddStringToObject(chat_config, "user_id", chat_obj->config.user_id);
    cJSON_AddItemToObject(chat_config, "meta_data", cJSON_CreateObject());
    cJSON_AddItemToObject(chat_config, "custom_variables", cJSON_CreateObject());
    cJSON_AddItemToObject(chat_config, "extra_params", cJSON_CreateObject());

    if (chat_obj->chat_parameters.cnt > 0) {
        cJSON *parameters = cJSON_CreateObject();
        for (int i = 0; i < chat_obj->chat_parameters.cnt; i++) {
            cJSON_AddStringToObject(parameters, chat_obj->chat_parameters.parameters[i].key, chat_obj->chat_parameters.parameters[i].value);
        }
        cJSON_AddItemToObject(chat_config, "parameters", parameters);
    }
    cJSON_AddItemToObject(data, "chat_config", chat_config);

    cJSON *input_audio = cJSON_CreateObject();
    cJSON_AddStringToObject(input_audio, "format", "pcm");
    if (chat_obj->config.uplink_audio_type == ESP_COZE_CHAT_AUDIO_TYPE_OPUS) {
        cJSON_AddStringToObject(input_audio, "codec", "opus");
    } else if (chat_obj->config.uplink_audio_type == ESP_COZE_CHAT_AUDIO_TYPE_G711A) {
        cJSON_AddStringToObject(input_audio, "codec", "g711a");
    } else if (chat_obj->config.uplink_audio_type == ESP_COZE_CHAT_AUDIO_TYPE_G711U) {
        cJSON_AddStringToObject(input_audio, "codec", "g711u");
    } else {
        cJSON_AddStringToObject(input_audio, "codec", "pcm");
    }
    if (chat_obj->config.uplink_audio_type == ESP_COZE_CHAT_AUDIO_TYPE_G711A ||
            chat_obj->config.uplink_audio_type == ESP_COZE_CHAT_AUDIO_TYPE_G711U) {
        cJSON_AddNumberToObject(input_audio, "sample_rate", 8000);
    } else {
        cJSON_AddNumberToObject(input_audio, "sample_rate", 16000);
    }
    cJSON_AddNumberToObject(input_audio, "channel", 1);
    cJSON_AddNumberToObject(input_audio, "bit_depth", 16);
    cJSON_AddItemToObject(data, "input_audio", input_audio);

    if (chat_obj->config.mode == ESP_COZE_CHAT_SPEECH_INTERRUPT_MODE) {
        cJSON *turn_detection = cJSON_CreateObject();
        cJSON_AddStringToObject(turn_detection, "type", "server_vad");
        cJSON_AddNumberToObject(turn_detection, "prefix_padding_ms", 600);
        cJSON_AddNumberToObject(turn_detection, "silence_duration_ms", 500);
        cJSON_AddItemToObject(data, "turn_detection", turn_detection);
    }

    cJSON *output_audio = cJSON_CreateObject();
    if (chat_obj->config.downlink_audio_type == ESP_COZE_CHAT_AUDIO_TYPE_OPUS) {
        cJSON_AddStringToObject(output_audio, "codec", "opus");
    } else if (chat_obj->config.downlink_audio_type == ESP_COZE_CHAT_AUDIO_TYPE_G711A) {
        cJSON_AddStringToObject(output_audio, "codec", "g711a");
    } else if (chat_obj->config.downlink_audio_type == ESP_COZE_CHAT_AUDIO_TYPE_G711U) {
        cJSON_AddStringToObject(output_audio, "codec", "g711u");
    } else {
        cJSON_AddStringToObject(output_audio, "codec", "pcm");
    }

    if (chat_obj->config.downlink_audio_type == ESP_COZE_CHAT_AUDIO_TYPE_OPUS) {
        cJSON *opus_config = cJSON_CreateObject();
        cJSON_AddNumberToObject(opus_config, "bitrate", 16000);
        cJSON_AddNumberToObject(opus_config, "frame_size_ms", 60);
        cJSON *pcm_limit_config = cJSON_CreateObject();
        cJSON_AddNumberToObject(pcm_limit_config, "period", 1);
        cJSON_AddNumberToObject(pcm_limit_config, "max_frame_num", 18);
        cJSON_AddItemToObject(opus_config, "limit_config", pcm_limit_config);
        cJSON_AddItemToObject(output_audio, "opus_config", opus_config);
    } else {
        cJSON *pcm_config = cJSON_CreateObject();
        if (chat_obj->config.downlink_audio_type == ESP_COZE_CHAT_AUDIO_TYPE_PCM) {
            cJSON_AddNumberToObject(pcm_config, "sample_rate", 16000);
        } else {
            cJSON_AddNumberToObject(pcm_config, "sample_rate", 8000);
        }
        cJSON_AddNumberToObject(pcm_config, "frame_size_ms", 60);
        cJSON *pcm_limit_config = cJSON_CreateObject();
        cJSON_AddNumberToObject(pcm_limit_config, "period", 1);
        cJSON_AddNumberToObject(pcm_limit_config, "max_frame_num", 25);
        cJSON_AddItemToObject(pcm_config, "limit_config", pcm_limit_config);
        cJSON_AddItemToObject(output_audio, "pcm_config", pcm_config);
    }
    cJSON_AddNumberToObject(output_audio, "speech_rate", 20);
    cJSON_AddStringToObject(output_audio, "voice_id", chat_obj->config.voice_id);

    cJSON_AddItemToObject(data, "output_audio", output_audio);

    cJSON *event_sub = cJSON_CreateArray();
    cJSON_AddItemToArray(event_sub, cJSON_CreateString("conversation.audio.delta"));
    cJSON_AddItemToArray(event_sub, cJSON_CreateString("conversation.chat.completed"));
    cJSON_AddItemToArray(event_sub, cJSON_CreateString("input_audio_buffer.speech_started"));
    cJSON_AddItemToArray(event_sub, cJSON_CreateString("input_audio_buffer.speech_stopped"));
    cJSON_AddItemToArray(event_sub, cJSON_CreateString("conversation.chat.requires_action"));
    cJSON_AddItemToArray(event_sub, cJSON_CreateString("chat.created"));
    cJSON_AddItemToArray(event_sub, cJSON_CreateString("error"));

    if (chat_obj->config.enable_subtitle) {
        cJSON_AddItemToArray(event_sub, cJSON_CreateString("conversation.message.delta"));
    }
    if (chat_obj->config.subscribe_event) {
        int sub_index = 0;
        while (chat_obj->config.subscribe_event[sub_index] != NULL) {
            cJSON_AddItemToArray(event_sub, cJSON_CreateString(chat_obj->config.subscribe_event[sub_index]));
            sub_index++;
        }
    }
    cJSON_AddItemToObject(data, "event_subscriptions", event_sub);

    char *json_string = cJSON_Print(root);
    ESP_LOGI(TAG, "Update chat: %s", json_string);

    ret = websocket_send_with_retry(chat_obj->client, json_string, strlen(json_string),
                                    3, 50, "Update chat");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Update chat failed: %s", esp_err_to_name(ret));
    }
    free(json_string);
    cJSON_Delete(root);
    ESP_COZE_CHAT_UNLOCK(chat_obj);
    return ret;
}

esp_err_t esp_coze_chat_send_customer_data(esp_coze_chat_handle_t chat_hd, const char *data)
{
    if (!chat_hd || !data) {
        ESP_LOGE(TAG, "Invalid parameters: chat_hd=%p, data=%p", chat_hd, data);
        return ESP_ERR_INVALID_ARG;
    }

    coze_chat_t *chat_obj = (coze_chat_t *)chat_hd;
    ESP_COZE_CHAT_LOCK(chat_obj);

    esp_err_t ret = websocket_send_with_retry(chat_obj->client, data, strlen(data),
                    3, 50, "Customer data");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send customer data failed: %s", esp_err_to_name(ret));
    }

    ESP_COZE_CHAT_UNLOCK(chat_obj);
    return ret;
}
