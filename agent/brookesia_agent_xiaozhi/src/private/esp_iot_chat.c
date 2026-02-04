/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <ctype.h>

#include <esp_check.h>
#include <esp_log.h>
#include <esp_app_desc.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_tls.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "esp_iot_settings.h"
#include "esp_iot_board.h"
#include "esp_iot_chat.h"
#include "esp_iot_mqtt.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
#define MAX_UDP_RECEIVE_BUFFER 2048
#define MAX_CONSECUTIVE_UDP_ERRORS 10
#define UDP_RECEIVE_TASK_STACK_SIZE (6 * 1024)
#define UDP_RECEIVE_TASK_PRIORITY 5
#define HTTP_REQUEST_TIMEOUT_MS 10000

/**
 * @brief  Queue item for IoT chat
 */
typedef struct {
    char *data;                     /*!< Message data */
    size_t data_len;                /*!< Data length */
    char *topic;                    /*!< MQTT topic */
    size_t topic_len;               /*!< Topic length */
} esp_iot_chat_queue_item_t;

/**
 * @brief  IoT chat
 */
typedef struct {
    board_handle_t board;

    int audio_input_task_core;
    uint8_t audio_input_task_priority;
    uint32_t audio_input_task_stack_size;
    bool audio_input_task_stack_in_ext;

    bool error_occurred;                                /*!< Error occurred */
    bool mqtt_connected;                                /*!< MQTT connected */
    bool audio_channel_opened;                          /*!< Audio channel opened */
    esp_iot_chat_device_state_t device_state;           /*!< Device state */
    esp_iot_chat_listening_mode_t listening_mode;       /*!< Listening mode */

    esp_iot_mqtt_handle_t mqtt_handle;                  /*!< MQTT handle */
    char publish_topic[ESP_IOT_MQTT_MAX_STRING_SIZE];   /*!< Publish topic */
    char subscribe_topic[ESP_IOT_MQTT_MAX_STRING_SIZE]; /*!< Subscribe topic */

    EventGroupHandle_t event_group_handle;              /*!< Event group for handling events */

    SemaphoreHandle_t channel_mutex;                    /*!< Channel mutex for thread safety */
    TaskHandle_t audio_input_task_handle;               /*!< Audio input task handle */
    int udp_socket;                                     /*!< UDP socket for audio streaming */
    struct sockaddr_in udp_addr;                        /*!< UDP address */
    char udp_server[ESP_IOT_MQTT_MAX_STRING_SIZE];      /*!< UDP server */
    int udp_port;                                       /*!< UDP port */

    mbedtls_aes_context aes_ctx;                        /*!< AES context */
    uint8_t aes_nonce[ESP_IOT_MQTT_AES_NONCE_SIZE];     /*!< AES nonce */

    char session_id[ESP_IOT_MQTT_MAX_STRING_SIZE];      /*!< Session ID */
    uint32_t local_sequence;                            /*!< Local sequence */
    uint32_t remote_sequence;                           /*!< Remote sequence */
    int server_sample_rate;                             /*!< Server sample rate */
    int server_frame_duration;                          /*!< Server frame duration */
    uint32_t last_incoming_time;                        /*!< Last incoming time */

    esp_iot_chat_audio_callback_t   audio_callback;     /*!< Callback function for handling audio data */
    esp_iot_chat_event_callback_t   event_callback;     /*!< Callback function for handling events */
    void                           *audio_callback_ctx; /*!< Context pointer passed to the audio callback */
    void                           *event_callback_ctx; /*!< Context pointer passed to the event callback */
} esp_iot_chat_t;

/**
 * @brief  Type handler for IoT chat
 */
typedef esp_err_t (*esp_iot_chat_type_handler_t)(esp_iot_chat_t *iot_chat, cJSON *root);

/**
 * @brief  Type function for IoT chat
 */
typedef struct {
    const char *type;
    esp_iot_chat_type_handler_t handler;
} esp_iot_chat_type_func_t;

ESP_EVENT_DEFINE_BASE(ESP_IOT_CHAT_EVENTS);

static const char *TAG = "esp_iot_chat";
static esp_iot_chat_handle_t s_iot_chat_handle;

static uint8_t char_to_hex(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return 0;
}

static void esp_iot_chat_decode_hex_string(const char *hex_string, uint8_t *output, size_t output_size)
{
    if (!hex_string || !output) {
        return;
    }

    size_t hex_len = strlen(hex_string);
    size_t decode_len = hex_len / 2;
    if (decode_len > output_size) {
        decode_len = output_size;
    }

    for (size_t i = 0; i < decode_len; i++) {
        output[i] = (char_to_hex(hex_string[i * 2]) << 4) | char_to_hex(hex_string[i * 2 + 1]);
    }
}

static char *str_dup(const char *str)
{
    if (!str) {
        return NULL;
    }
    size_t len = strlen(str) + 1;
    char *dup = malloc(len);
    if (dup) {
        memcpy(dup, str, len);
    }
    return dup;
}

static void str_free(char **str)
{
    if (str && *str) {
        free(*str);
        *str = NULL;
    }
}

static int *esp_iot_chat_ota_parse_version(const char *version, size_t *count)
{
    if (!version || !count) {
        return NULL;
    }

    *count = 0;

    size_t dots = 0;
    for (const char *p = version; *p; p++) {
        if (*p == '.') {
            dots++;
        }
    }

    size_t components = dots + 1;
    int *version_numbers = malloc(components * sizeof(int));
    if (!version_numbers) {
        return NULL;
    }

    char *version_copy = str_dup(version);
    if (!version_copy) {
        free(version_numbers);
        return NULL;
    }

    char *token = strtok(version_copy, ".");
    size_t i = 0;

    while (token && i < components) {
        version_numbers[i] = atoi(token);
        token = strtok(NULL, ".");
        i++;
    }

    *count = i;
    free(version_copy);
    return version_numbers;
}

static void esp_iot_chat_ota_free(int *version_array)
{
    if (version_array) {
        free(version_array);
    }
}

static bool esp_iot_chat_ota_available(const char *current_version, const char *new_version)
{
    if (!current_version || !new_version) {
        return false;
    }

    size_t current_count, new_count;
    int *current_nums = esp_iot_chat_ota_parse_version(current_version, &current_count);
    int *new_nums = esp_iot_chat_ota_parse_version(new_version, &new_count);

    if (!current_nums || !new_nums) {
        esp_iot_chat_ota_free(current_nums);
        esp_iot_chat_ota_free(new_nums);
        return false;
    }

    size_t min_count = (current_count < new_count) ? current_count : new_count;

    for (size_t i = 0; i < min_count; i++) {
        if (new_nums[i] > current_nums[i]) {
            esp_iot_chat_ota_free(current_nums);
            esp_iot_chat_ota_free(new_nums);
            return true;
        } else if (new_nums[i] < current_nums[i]) {
            esp_iot_chat_ota_free(current_nums);
            esp_iot_chat_ota_free(new_nums);
            return false;
        }
    }

    bool result = new_count > current_count;
    esp_iot_chat_ota_free(current_nums);
    esp_iot_chat_ota_free(new_nums);
    return result;
}

static esp_err_t esp_iot_chat_http_data_handler(const char *data, size_t data_len, esp_iot_chat_http_info_t *info)
{
    cJSON *root = cJSON_Parse(data);
    ESP_RETURN_ON_ERROR(root == NULL, TAG, "Failed to parse JSON response");

    cJSON *activation = cJSON_GetObjectItem(root, "activation");
    if (cJSON_IsObject(activation)) {
        cJSON *message = cJSON_GetObjectItem(activation, "message");
        if (cJSON_IsString(message)) {
            ESP_LOGI(TAG, "activation message: %s", message->valuestring);
            str_free(&info->activation_message);
            info->activation_message = str_dup(message->valuestring);
        }
        cJSON *code = cJSON_GetObjectItem(activation, "code");
        if (cJSON_IsString(code)) {
            ESP_LOGI(TAG, "activation code: %s", code->valuestring);
            str_free(&info->activation_code);
            info->activation_code = str_dup(code->valuestring);
            info->has_activation_code = true;
        }
        cJSON *challenge = cJSON_GetObjectItem(activation, "challenge");
        if (cJSON_IsString(challenge)) {
            ESP_LOGI(TAG, "activation challenge: %s", challenge->valuestring);
            str_free(&info->activation_challenge);
            info->activation_challenge = str_dup(challenge->valuestring);
            info->has_activation_challenge = true;
        }
        cJSON *timeout_ms = cJSON_GetObjectItem(activation, "timeout_ms");
        if (cJSON_IsNumber(timeout_ms)) {
            ESP_LOGI(TAG, "activation timeout_ms: %d", timeout_ms->valueint);
            info->activation_timeout_ms = timeout_ms->valueint;
        }
    }

    info->has_mqtt_config = false;
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (cJSON_IsObject(mqtt)) {
        settings_handle_t settings;
        esp_err_t settings_ret = settings_init(&settings, "mqtt", true);
        ESP_RETURN_ON_ERROR(settings_ret, TAG, "Failed to initialize settings: %s", esp_err_to_name(settings_ret));

        cJSON *item = NULL;
        cJSON_ArrayForEach(item, mqtt) {
            if (cJSON_IsString(item)) {
                settings_set_string(&settings, item->string, item->valuestring);
            }
        }
        info->has_mqtt_config = true;
    } else {
        ESP_LOGE(TAG, "No mqtt section found!");
    }

    info->has_websocket_config = false;
    cJSON *websocket = cJSON_GetObjectItem(root, "websocket");
    if (cJSON_IsObject(websocket)) {
        settings_handle_t settings;
        esp_err_t settings_ret = settings_init(&settings, "websocket", true);
        ESP_RETURN_ON_ERROR(settings_ret, TAG, "Failed to initialize settings: %s", esp_err_to_name(settings_ret));

        cJSON *item = NULL;
        cJSON_ArrayForEach(item, websocket) {
            if (cJSON_IsString(item)) {
                settings_set_string(&settings, item->string, item->valuestring);
            } else if (cJSON_IsNumber(item)) {
                settings_set_int(&settings, item->string, item->valueint);
            }
        }
        info->has_websocket_config = true;
    } else {
        ESP_LOGE(TAG, "No websocket section found!");
    }

    // info->has_server_time = false;
    // cJSON *server_time = cJSON_GetObjectItem(root, "server_time");
    // if (cJSON_IsObject(server_time)) {
    //     cJSON *timestamp = cJSON_GetObjectItem(server_time, "timestamp");
    //     cJSON *timezone_offset = cJSON_GetObjectItem(server_time, "timezone_offset");

    //     if (cJSON_IsNumber(timestamp)) {
    //         struct timeval tv;
    //         double ts = timestamp->valuedouble;
    //         if (cJSON_IsNumber(timezone_offset)) {
    //             ts += (timezone_offset->valueint * 60 * 1000);
    //         }

    //         tv.tv_sec = (time_t)(ts / 1000);
    //         tv.tv_usec = (suseconds_t)((long long)ts % 1000) * 1000;
    //         settimeofday(&tv, NULL);
    //         info->has_server_time = true;

    //         struct tm timeinfo;
    //         localtime_r(&tv.tv_sec, &timeinfo);
    //         ESP_LOGW(TAG, "Update server time: %04d-%02d-%02d %02d:%02d:%02d.%03d",
    //                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
    //                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, (int)(tv.tv_usec / 1000));
    //     }
    // } else {
    //     ESP_LOGW(TAG, "No server_time section found!");
    // }

    info->has_new_version = false;
    cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    if (cJSON_IsObject(firmware)) {
        cJSON *version = cJSON_GetObjectItem(firmware, "version");
        if (cJSON_IsString(version)) {
            str_free(&info->firmware_version);
            info->firmware_version = str_dup(version->valuestring);
        }

        cJSON *firmware_url = cJSON_GetObjectItem(firmware, "url");
        if (cJSON_IsString(firmware_url)) {
            str_free(&info->firmware_url);
            info->firmware_url = str_dup(firmware_url->valuestring);
        }

        if (cJSON_IsString(version) && cJSON_IsString(firmware_url)) {
            info->has_new_version = esp_iot_chat_ota_available(info->current_version, info->firmware_version);
            if (info->has_new_version) {
                ESP_LOGI(TAG, "New version available: %s", info->firmware_version);
            } else {
                ESP_LOGI(TAG, "Current is the latest version");
            }

            cJSON *force = cJSON_GetObjectItem(firmware, "force");
            if (cJSON_IsNumber(force) && force->valueint == 1) {
                info->has_new_version = true;
            }
        }
    } else {
        ESP_LOGW(TAG, "No firmware section found!");
    }

    cJSON_Delete(root);

    return ESP_OK;
}

static esp_err_t esp_iot_chat_http_event(esp_http_client_event_t *evt)
{
    static char *output_buffer = NULL;
    static int output_len = 0;
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        // if (output_len == 0 && evt->user_data) {
        //     memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
        // }

        if (!esp_http_client_is_chunked_response(evt->client)) {
            int copy_len = 0;
            // if (evt->user_data) {
            //     copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
            //     if (copy_len) {
            //         memcpy(evt->user_data + output_len, evt->data, copy_len);
            //     }
            // } else {
            int content_len = esp_http_client_get_content_length(evt->client);
            if (!output_buffer) {
                output_len = 0;
                output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                ESP_RETURN_ON_FALSE(output_buffer, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for output buffer");
            }

            copy_len = MIN(evt->data_len, (content_len - output_len));
            if (copy_len) {
                memcpy(output_buffer + output_len, evt->data, copy_len);
            }
            // }
            output_len += copy_len;
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL) {
            esp_iot_chat_http_info_t *info = (esp_iot_chat_http_info_t *)evt->user_data;
            esp_iot_chat_http_data_handler(output_buffer, output_len, info);
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0) {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        if (output_buffer != NULL) {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_redirection(evt->client);
        break;
    }
    return ESP_OK;
}

esp_err_t esp_iot_chat_get_http_info(esp_iot_chat_handle_t chat_hd, esp_iot_chat_http_info_t *info)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;
    board_handle_t board = iot_chat->board;

    char uuid_str[64] = {0};
    char mac_str[18] = {0};
    esp_err_t ret = ESP_OK;

    ret = board_get_uuid(&board, uuid_str, sizeof(uuid_str));
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to get UUID, status: %s", esp_err_to_name(ret));

    ret = board_get_mac_address(mac_str);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to get MAC address, status: %s", esp_err_to_name(ret));

    char *json_buffer = calloc(1, 4096);
    ESP_RETURN_ON_ERROR(json_buffer == NULL, TAG, "Failed to allocate memory for JSON buffer");
    ret = board_get_json(&board, json_buffer, 4096);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to get full JSON, status: %s", esp_err_to_name(ret));

    esp_http_client_config_t http_client_cfg = {
        .url = CONFIG_BROOKESIA_AGENT_XIAOZHI_OTA_URL,
        .event_handler = esp_iot_chat_http_event,
        .user_data = info,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = HTTP_REQUEST_TIMEOUT_MS,
    };

    esp_http_client_handle_t http_client = esp_http_client_init(&http_client_cfg);
    ESP_RETURN_ON_ERROR(http_client == NULL, TAG, "Failed to initialize HTTP client");

    esp_http_client_set_header(http_client, "Client-Id", uuid_str);
    esp_http_client_set_header(http_client, "Device-Id", mac_str);
    esp_http_client_set_header(http_client, "User-Agent", CONFIG_IDF_TARGET);
    esp_http_client_set_header(http_client, "Accept-Language", "zh-CN");
    esp_http_client_set_header(http_client, "Activation-Version", "1");
    esp_http_client_set_header(http_client, "Content-Type", "application/json");

    esp_http_client_set_method(http_client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(http_client, json_buffer, strlen(json_buffer));

    while (1) {
        ret = esp_http_client_perform(http_client);
        if (ret != ESP_ERR_HTTP_EAGAIN) {
            break;
        }
    }

    esp_http_client_cleanup(http_client);
    free(json_buffer);

    return ret;
}

void esp_iot_chat_free_http_info(esp_iot_chat_http_info_t *info)
{
    if (info == NULL) {
        return;
    }

    str_free(&info->current_version);
    str_free(&info->firmware_version);
    str_free(&info->firmware_url);
    str_free(&info->serial_number);
    str_free(&info->activation_code);
    str_free(&info->activation_challenge);
    str_free(&info->activation_message);
}

static bool esp_iot_chat_is_audio_ready(esp_iot_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, false, TAG, "Invalid handle");

    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;

    return iot_chat->audio_channel_opened &&
           iot_chat->udp_socket != -1 &&
           !iot_chat->error_occurred;
}

static esp_err_t esp_iot_chat_set_listening_mode(esp_iot_chat_t *iot_chat, esp_iot_chat_listening_mode_t mode)
{
    const char *const mode_str[] = {
        "unknown",
        "realtime",
        "auto",
        "manual",
        "manual_stop",
        "auto_stop"
    };

    if (iot_chat->listening_mode == mode) {
        ESP_LOGW(TAG, "Listening mode is already %s", mode_str[mode]);
        return ESP_OK;
    }

    iot_chat->listening_mode = mode;
    ESP_LOGI(TAG, "Listening mode: %s", mode_str[mode]);

    return ESP_OK;
}

static esp_err_t esp_iot_chat_set_device_state(esp_iot_chat_t *iot_chat, esp_iot_chat_device_state_t state)
{
    if (iot_chat->device_state == state) {
        ESP_LOGW(TAG, "Device state(%d) is already", state);
        return ESP_OK;
    }

    iot_chat->device_state = state;
    ESP_LOGI(TAG, "Device state: %d", iot_chat->device_state);

    if (iot_chat->event_callback) {
        iot_chat->event_callback(ESP_IOT_CHAT_EVENT_CHAT_STATE, &state, iot_chat->event_callback_ctx);
    }

    return ESP_OK;
}

static esp_err_t esp_iot_chat_hello_handler(esp_iot_chat_t *iot_chat, cJSON *root)
{
    const cJSON *transport = cJSON_GetObjectItem(root, "transport");
    if (!transport || !cJSON_IsString(transport) || strcmp(transport->valuestring, "udp") != 0) {
        ESP_LOGE(TAG, "Unsupported transport: %s", transport ? transport->valuestring : "null");
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *session_id = cJSON_GetObjectItem(root, "session_id");
    if (cJSON_IsString(session_id)) {
        strncpy(iot_chat->session_id, session_id->valuestring, sizeof(iot_chat->session_id) - 1);
        iot_chat->session_id[sizeof(iot_chat->session_id) - 1] = '\0';
        ESP_LOGI(TAG, "Session ID: %s", iot_chat->session_id);
    }

    const cJSON *audio_params = cJSON_GetObjectItem(root, "audio_params");
    if (cJSON_IsObject(audio_params)) {
        const cJSON *sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        if (cJSON_IsNumber(sample_rate)) {
            iot_chat->server_sample_rate = sample_rate->valueint;
        }

        const cJSON *frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
        if (cJSON_IsNumber(frame_duration)) {
            iot_chat->server_frame_duration = frame_duration->valueint;
        }
    }

    const cJSON *udp = cJSON_GetObjectItem(root, "udp");
    if (!cJSON_IsObject(udp)) {
        ESP_LOGE(TAG, "UDP is not specified");
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *server = cJSON_GetObjectItem(udp, "server");
    const cJSON *port = cJSON_GetObjectItem(udp, "port");
    const cJSON *key = cJSON_GetObjectItem(udp, "key");
    const cJSON *nonce = cJSON_GetObjectItem(udp, "nonce");

    if (server && cJSON_IsString(server)) {
        strncpy(iot_chat->udp_server, server->valuestring, sizeof(iot_chat->udp_server) - 1);
        iot_chat->udp_server[sizeof(iot_chat->udp_server) - 1] = '\0';
    }

    if (port && cJSON_IsNumber(port)) {
        iot_chat->udp_port = port->valueint;
    }

    if (key && cJSON_IsString(key) && nonce && cJSON_IsString(nonce)) {
        uint8_t aes_key[ESP_IOT_MQTT_AES_KEY_SIZE];
        esp_iot_chat_decode_hex_string(nonce->valuestring, iot_chat->aes_nonce, ESP_IOT_MQTT_AES_NONCE_SIZE);
        esp_iot_chat_decode_hex_string(key->valuestring, aes_key, ESP_IOT_MQTT_AES_KEY_SIZE);

        mbedtls_aes_init(&iot_chat->aes_ctx);
        mbedtls_aes_setkey_enc(&iot_chat->aes_ctx, aes_key, 128);

        iot_chat->local_sequence = 0;
        iot_chat->remote_sequence = 0;
    }

    xEventGroupSetBits(iot_chat->event_group_handle, ESP_IOT_CHAT_EVENT_SERVER_HELLO);

    return ESP_OK;
}

static esp_err_t esp_iot_chat_goodbye_handler(esp_iot_chat_t *iot_chat, cJSON *root)
{
    cJSON *session_id = cJSON_GetObjectItem(root, "session_id");
    ESP_LOGI(TAG, "Received goodbye message, session_id: %s", session_id ? session_id->valuestring : "null");
    if (session_id == NULL || strcmp(iot_chat->session_id, session_id->valuestring) == 0) {
        esp_event_post(ESP_IOT_CHAT_EVENTS, ESP_IOT_CHAT_EVENT_SERVER_GOODBYE, NULL, 0, portMAX_DELAY);
    }

    return ESP_OK;
}

static esp_err_t esp_iot_chat_mcp_handler(esp_iot_chat_t *iot_chat, cJSON *root)
{
    cJSON *payload_obj = cJSON_GetObjectItem(root, "payload");
    char *payload_mcp = NULL;
    if (payload_obj && cJSON_IsObject(payload_obj)) {
        cJSON *method = cJSON_GetObjectItem(payload_obj, "method");
        if (cJSON_IsString(method)) {
            if (!strncmp(method->valuestring, "notifications", strlen("notifications"))) {
                ESP_LOGW(TAG, "MCP occurred notifications");
            } else {
                payload_mcp = cJSON_PrintUnformatted(payload_obj);
                ESP_LOGI(TAG, "MCP payload: %s", payload_mcp);
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to print payload");
    }

    return ESP_OK;
}

static esp_err_t esp_iot_chat_tts_handler(esp_iot_chat_t *iot_chat, cJSON *root)
{
    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (cJSON_IsString(state)) {
        ESP_LOGI(TAG, "TTS state: %s", state->valuestring);
        if (strcmp(state->valuestring, "start") == 0) {
            ESP_LOGI(TAG, "TTS started");
            if (iot_chat->device_state == ESP_IOT_CHAT_DEVICE_STATE_IDLE ||
                    iot_chat->device_state == ESP_IOT_CHAT_DEVICE_STATE_LISTENING) {
                esp_iot_chat_set_device_state(iot_chat, ESP_IOT_CHAT_DEVICE_STATE_SPEAKING);
            }
        } else if (strcmp(state->valuestring, "stop") == 0) {
            ESP_LOGI(TAG, "TTS stopped");
            if (iot_chat->device_state == ESP_IOT_CHAT_DEVICE_STATE_SPEAKING) {
                if (iot_chat->listening_mode == ESP_IOT_CHAT_LISTENING_MODE_MANUAL) {
                    esp_iot_chat_set_device_state(iot_chat, ESP_IOT_CHAT_DEVICE_STATE_IDLE);
                } else {
                    esp_iot_chat_set_device_state(iot_chat, ESP_IOT_CHAT_DEVICE_STATE_LISTENING);
                }
            }
        } else if (strcmp(state->valuestring, "sentence_start") == 0) {
            ESP_LOGI(TAG, "TTS sentence started");
            cJSON *text = cJSON_GetObjectItem(root, "text");
            if (cJSON_IsString(text)) {
                ESP_LOGI(TAG, "TTS sentence text: %s", text->valuestring);
                esp_iot_chat_text_data_t text_data = {
                    .role = ESP_IOT_CHAT_TEXT_ROLE_ASSISTANT,
                    .text = text->valuestring,
                };
                if (iot_chat->event_callback) {
                    iot_chat->event_callback(ESP_IOT_CHAT_EVENT_CHAT_TEXT, &text_data, iot_chat->event_callback_ctx);
                }
            }
        }
    }

    return ESP_OK;
}

static esp_err_t esp_iot_chat_stt_handler(esp_iot_chat_t *iot_chat, cJSON *root)
{
    cJSON *text = cJSON_GetObjectItem(root, "text");
    if (cJSON_IsString(text)) {
        ESP_LOGI(TAG, "STT text: %s", text->valuestring);
        esp_iot_chat_text_data_t text_data = {
            .role = ESP_IOT_CHAT_TEXT_ROLE_USER,
            .text = text->valuestring,
        };
        if (iot_chat->event_callback) {
            iot_chat->event_callback(ESP_IOT_CHAT_EVENT_CHAT_TEXT, &text_data, iot_chat->event_callback_ctx);
        }
    }

    return ESP_OK;
}

static esp_err_t esp_iot_chat_llm_handler(esp_iot_chat_t *iot_chat, cJSON *root)
{
    cJSON *emotion = cJSON_GetObjectItem(root, "emotion");
    if (cJSON_IsString(emotion)) {
        ESP_LOGI(TAG, "LLM emotion: %s", emotion->valuestring);
        if (iot_chat->event_callback) {
            iot_chat->event_callback(ESP_IOT_CHAT_EVENT_CHAT_EMOJI, emotion->valuestring, iot_chat->event_callback_ctx);
        }
    }

    return ESP_OK;
}

static esp_err_t esp_iot_chat_system_handler(esp_iot_chat_t *iot_chat, cJSON *root)
{
    cJSON *command = cJSON_GetObjectItem(root, "command");
    if (cJSON_IsString(command)) {
        ESP_LOGI(TAG, "System command: %s", command->valuestring);
        if (strcmp(command->valuestring, "reboot") == 0) {
            esp_restart();
        }
    }

    return ESP_OK;
}

static esp_err_t esp_iot_chat_data_handler(esp_iot_mqtt_handle_t handle, const char *topic, size_t topic_len, const char *data, size_t data_len)
{
    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)s_iot_chat_handle;
    if (!iot_chat) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(data);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON message: %.*s", (int)data_len, data);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type)) {
        ESP_LOGE(TAG, "Message type is invalid");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    esp_iot_chat_type_func_t esp_iot_chat_type_handlers[] = {
        { "hello",   esp_iot_chat_hello_handler },
        { "goodbye", esp_iot_chat_goodbye_handler },
        { "mcp",     esp_iot_chat_mcp_handler },
        { "tts",     esp_iot_chat_tts_handler },
        { "stt",     esp_iot_chat_stt_handler },
        { "llm",     esp_iot_chat_llm_handler },
        { "system",  esp_iot_chat_system_handler },
    };

    for (size_t i = 0; i < sizeof(esp_iot_chat_type_handlers) / sizeof(esp_iot_chat_type_handlers[0]); i++) {
        if (strcmp(type->valuestring, esp_iot_chat_type_handlers[i].type) == 0) {
            ret = esp_iot_chat_type_handlers[i].handler(iot_chat, root);
            break;
        }
    }

    cJSON_Delete(root);

    return ret;
}

static esp_err_t esp_iot_chat_connected_handler(esp_iot_mqtt_handle_t handle)
{
    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)s_iot_chat_handle;
    ESP_RETURN_ON_FALSE(iot_chat, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    iot_chat->mqtt_connected = true;

    esp_event_post(ESP_IOT_CHAT_EVENTS, ESP_IOT_CHAT_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);

    return ESP_OK;
}

static esp_err_t esp_iot_chat_disconnected_handler(esp_iot_mqtt_handle_t handle)
{
    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)s_iot_chat_handle;
    ESP_RETURN_ON_FALSE(iot_chat, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    iot_chat->mqtt_connected = false;

    esp_event_post(ESP_IOT_CHAT_EVENTS, ESP_IOT_CHAT_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY);

    return ESP_OK;
}

static esp_err_t esp_iot_chat_send_json_message(esp_iot_chat_handle_t chat_hd, cJSON *root)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(root, ESP_ERR_INVALID_ARG, TAG, "Invalid root");

    esp_err_t ret = ESP_OK;
    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;
    char *json_str = NULL;

    json_str = cJSON_PrintUnformatted(root);
    ESP_RETURN_ON_FALSE(json_str, ESP_ERR_NO_MEM, TAG, "Failed to print JSON");

    ret = esp_iot_mqtt_send_text(iot_chat->mqtt_handle, json_str);
    cJSON_free(json_str);

    return ret;
}

static esp_err_t esp_iot_chat_get_hello_message(esp_iot_chat_handle_t chat_hd, char *message, size_t message_len)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "hello");
    cJSON_AddNumberToObject(root, "version", 3);
    cJSON_AddStringToObject(root, "transport", "udp");

    cJSON *features = cJSON_CreateObject();
#ifdef CONFIG_USE_SERVER_AEC
    cJSON_AddBoolToObject(features, "aec", true);
#endif

    cJSON_AddBoolToObject(features, "mcp", false);
    cJSON_AddItemToObject(root, "features", features);

    cJSON *audio_params = cJSON_CreateObject();
    cJSON_AddStringToObject(audio_params, "format", "opus");
    cJSON_AddNumberToObject(audio_params, "sample_rate", 16000);
    cJSON_AddNumberToObject(audio_params, "channels", 1);
    cJSON_AddNumberToObject(audio_params, "frame_duration", 60);
    cJSON_AddItemToObject(root, "audio_params", audio_params);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json_str == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(message, json_str, message_len - 1);
    message[message_len - 1] = '\0';
    cJSON_free(json_str);

    return ESP_OK;
}

static void esp_iot_chat_audio_input(void *pvParameter)
{
    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)pvParameter;
    if (!iot_chat) {
        ESP_LOGE(TAG, "Invalid parameter in audio input task");
        vTaskDelete(NULL);
        return;
    }

    char *buffer = calloc(1, MAX_UDP_RECEIVE_BUFFER);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for audio input buffer");
        vTaskDelete(NULL);
        return;
    }

    uint8_t *decrypted_data = NULL;

    int consecutive_errors = 0;

    while (true) {
        // Cache socket fd to avoid race condition with esp_iot_chat_audio_close
        int sockfd = iot_chat->udp_socket;
        if (sockfd == -1) {
            break;
        }

        int received = recv(sockfd, buffer, MAX_UDP_RECEIVE_BUFFER, 0);

        if (received <= 0) {
            if (iot_chat->udp_socket == -1) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }

            consecutive_errors ++;
            ESP_LOGE(TAG, "Audio input error: %s (error count: %d, %d)", strerror(errno), consecutive_errors, iot_chat->udp_socket);

            if (consecutive_errors >= MAX_CONSECUTIVE_UDP_ERRORS) {
                ESP_LOGE(TAG, "Too many consecutive audio input errors, stopping input task");
                iot_chat->error_occurred = true;
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        consecutive_errors = 0;
        if (received < ESP_IOT_MQTT_AES_NONCE_SIZE) {
            ESP_LOGE(TAG, "Invalid audio packet size: %d", received);
            continue;
        }

        if (buffer[0] != 0x01) {
            ESP_LOGE(TAG, "Invalid audio packet type: %x", buffer[0]);
            continue;
        }

        uint32_t timestamp = ntohl(*(uint32_t *)&buffer[8]);
        uint32_t sequence = ntohl(*(uint32_t *)&buffer[12]);

        if (sequence < iot_chat->remote_sequence) {
            ESP_LOGW(TAG, "Received audio packet with old sequence: %lu, expected: %lu", sequence, iot_chat->remote_sequence);
            continue;
        }

        if (sequence != iot_chat->remote_sequence + 1) {
            ESP_LOGW(TAG, "Received audio packet with wrong sequence: %lu, expected: %lu", sequence, iot_chat->remote_sequence + 1);
        }

        size_t decrypted_size = received - ESP_IOT_MQTT_AES_NONCE_SIZE;
        decrypted_data = calloc(1, decrypted_size);
        if (!decrypted_data) {
            ESP_LOGE(TAG, "Failed to allocate memory for decrypted data");
            continue;
        }

        size_t nc_off = 0;
        uint8_t stream_block[16] = {0};
        const uint8_t *nonce = (const uint8_t *)buffer;
        const uint8_t *encrypted = (const uint8_t *)buffer + ESP_IOT_MQTT_AES_NONCE_SIZE;

        int ret = mbedtls_aes_crypt_ctr(&iot_chat->aes_ctx, decrypted_size, &nc_off, (uint8_t *)nonce, stream_block, (uint8_t *)encrypted, decrypted_data);
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to decrypt audio data, ret: %d", ret);
            free(decrypted_data);
            continue;
        }

        esp_iot_chat_audio_packet_t packet;
        packet.sample_rate = iot_chat->server_sample_rate;
        packet.frame_duration = iot_chat->server_frame_duration;
        packet.timestamp = timestamp;
        packet.payload = decrypted_data;
        packet.payload_size = decrypted_size;
        if (decrypted_size > 0 && iot_chat->audio_callback) {
            iot_chat->audio_callback(packet.payload, packet.payload_size, iot_chat->audio_callback_ctx);
        }

        free(decrypted_data);
        decrypted_data = NULL;
        iot_chat->remote_sequence = sequence;
        iot_chat->last_incoming_time = xTaskGetTickCount();
    }

    free(buffer);

    // Clear task handle before deleting task
    iot_chat->audio_input_task_handle = NULL;
    vTaskDelete(NULL);
}

static esp_err_t esp_iot_chat_audio_open(esp_iot_chat_t *iot_chat)
{
    ESP_RETURN_ON_FALSE(iot_chat, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    struct addrinfo *res = NULL;
    int sockfd = -1;
    esp_err_t ret = ESP_OK;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
    };
    char port_str[16] = {0};

    snprintf(port_str, sizeof(port_str), "%d", iot_chat->udp_port);
    if (getaddrinfo(iot_chat->udp_server, port_str, &hints, &res) != 0) {
        ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Failed to get address info: %s", strerror(errno));
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(res);
        ESP_RETURN_ON_ERROR(ESP_ERR_NO_MEM, TAG, "Failed to create socket: %s", strerror(errno));
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        close(sockfd);
        freeaddrinfo(res);
        ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Failed to connect audio channel: %s", strerror(errno));
    }

    if (xSemaphoreTake(iot_chat->channel_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        iot_chat->udp_socket = sockfd;
        iot_chat->audio_channel_opened = true;

        TaskHandle_t task_handle = NULL;
        BaseType_t task_ret = xTaskCreatePinnedToCoreWithCaps(
                                  esp_iot_chat_audio_input,
                                  "chat_audio",
                                  iot_chat->audio_input_task_stack_size,
                                  iot_chat,
                                  iot_chat->audio_input_task_priority,
                                  &task_handle,
                                  iot_chat->audio_input_task_core,
                                  (iot_chat->audio_input_task_stack_in_ext ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL) |
                                  MALLOC_CAP_8BIT
                              );
        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create audio input task");
            close(sockfd);
            iot_chat->udp_socket = -1;
            iot_chat->audio_channel_opened = false;
            ret = ESP_ERR_NO_MEM;
        } else {
            iot_chat->audio_input_task_handle = task_handle;
        }

        xSemaphoreGive(iot_chat->channel_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take channel mutex");
        close(sockfd);
        ret = ESP_ERR_TIMEOUT;
    }

    freeaddrinfo(res);
    return ret;
}

static esp_err_t esp_iot_chat_audio_close(esp_iot_chat_t *iot_chat)
{
    TaskHandle_t task_handle = NULL;

    if (xSemaphoreTake(iot_chat->channel_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        int sockfd = iot_chat->udp_socket;
        task_handle = iot_chat->audio_input_task_handle;

        if (sockfd != -1) {
            // Set udp_socket to -1 first to signal audio input task to exit
            iot_chat->udp_socket = -1;
            // Shutdown socket to interrupt blocking recv() in audio input task
            shutdown(sockfd, SHUT_RDWR);
            close(sockfd);
        }
        iot_chat->audio_channel_opened = false;
        iot_chat->local_sequence = 0;
        iot_chat->remote_sequence = 0;
        xSemaphoreGive(iot_chat->channel_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to take mutex for closing UDP socket");
    }

    // Wait for audio input task to exit (outside of mutex to avoid deadlock)
    if (task_handle != NULL) {
        const int max_wait_ms = 2000;
        const int poll_interval_ms = 10;
        int waited_ms = 0;

        while (iot_chat->audio_input_task_handle != NULL && waited_ms < max_wait_ms) {
            vTaskDelay(pdMS_TO_TICKS(poll_interval_ms));
            waited_ms += poll_interval_ms;
        }

        if (iot_chat->audio_input_task_handle != NULL) {
            ESP_LOGW(TAG, "Audio input task did not exit within %d ms", max_wait_ms);
        } else {
            ESP_LOGI(TAG, "Audio input task exited after %d ms", waited_ms);
        }
    }

    return ESP_OK;
}

esp_err_t esp_iot_chat_init(esp_iot_chat_config_t *config, esp_iot_chat_handle_t *chat_hd)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_iot_chat_t *iot_chat = calloc(1, sizeof(esp_iot_chat_t));
    ESP_RETURN_ON_FALSE(iot_chat, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory");

    *chat_hd = (esp_iot_chat_handle_t)iot_chat;

    iot_chat->audio_input_task_core = (config->audio_input_task_core < 0) ? tskNO_AFFINITY :
                                      (int)config->audio_input_task_core;
    iot_chat->audio_input_task_priority = config->audio_input_task_priority;
    iot_chat->audio_input_task_stack_size = config->audio_input_task_stack_size;
#if !CONFIG_SPIRAM
    iot_chat->audio_input_task_stack_in_ext = false;
#else
    iot_chat->audio_input_task_stack_in_ext = config->audio_input_task_stack_in_ext;
#endif

    iot_chat->event_group_handle = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(iot_chat->event_group_handle, ESP_ERR_NO_MEM, TAG, "Failed to create event group");

    iot_chat->channel_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(iot_chat->channel_mutex, ESP_ERR_NO_MEM, TAG, "Failed to create mutex");

    iot_chat->udp_socket = -1;
    iot_chat->audio_callback = config->audio_callback;
    iot_chat->event_callback = config->event_callback;
    iot_chat->audio_callback_ctx = config->audio_callback_ctx;
    iot_chat->event_callback_ctx = config->event_callback_ctx;

    esp_err_t ret = board_init(&iot_chat->board);
    ESP_GOTO_ON_ERROR(ret, error, TAG, "Failed to initialize board: %s", esp_err_to_name(ret));

    s_iot_chat_handle = (esp_iot_chat_handle_t)iot_chat;

    esp_iot_chat_set_device_state(iot_chat, ESP_IOT_CHAT_DEVICE_STATE_IDLE);
    esp_iot_chat_set_listening_mode(iot_chat, ESP_IOT_CHAT_LISTENING_MODE_AUTO);

    return ESP_OK;

error:

    board_cleanup(&iot_chat->board);
    free(iot_chat);

    return ret;
}

esp_err_t esp_iot_chat_deinit(esp_iot_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;

    esp_iot_chat_audio_close(iot_chat);

    if (iot_chat->mqtt_handle) {
        esp_iot_mqtt_stop(iot_chat->mqtt_handle);
        esp_iot_mqtt_deinit(iot_chat->mqtt_handle);
    }

    mbedtls_aes_free(&iot_chat->aes_ctx);

    if (iot_chat->channel_mutex) {
        vSemaphoreDelete(iot_chat->channel_mutex);
    }

    if (iot_chat->event_group_handle) {
        vEventGroupDelete(iot_chat->event_group_handle);
    }

    if (iot_chat->board.initialized) {
        board_cleanup(&iot_chat->board);
    }

    free(iot_chat);

    return ESP_OK;
}

esp_err_t esp_iot_chat_start(esp_iot_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;

    esp_iot_mqtt_init(&iot_chat->mqtt_handle);
    esp_iot_mqtt_callbacks_t callbacks = {
        .on_message_callback = esp_iot_chat_data_handler,
        .on_connected_callback = esp_iot_chat_connected_handler,
        .on_disconnected_callback = esp_iot_chat_disconnected_handler,
    };
    esp_iot_mqtt_set_callbacks(iot_chat->mqtt_handle, callbacks);

    return esp_iot_mqtt_start(iot_chat->mqtt_handle);
}

esp_err_t esp_iot_chat_stop(esp_iot_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;

    esp_iot_chat_close_audio_channel(chat_hd);
    esp_iot_mqtt_stop(iot_chat->mqtt_handle);

    return ESP_OK;
}

esp_err_t esp_iot_chat_open_audio_channel(esp_iot_chat_handle_t chat_hd, char *message, size_t message_len)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;

    if (message == NULL || message_len == 0) {
        message = calloc(1, 1024);
        ESP_RETURN_ON_FALSE(message, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory");
        esp_err_t ret = esp_iot_chat_get_hello_message(chat_hd, message, 1024);
        if (ret != ESP_OK) {
            free(message);
            message = NULL;
            ESP_RETURN_ON_ERROR(ret, TAG, "Failed to get hello message");
        }
    }

    iot_chat->error_occurred = false;
    memset(iot_chat->session_id, 0, sizeof(iot_chat->session_id));
    xEventGroupClearBits(iot_chat->event_group_handle, ESP_IOT_CHAT_EVENT_SERVER_HELLO);
    esp_iot_mqtt_send_text(iot_chat->mqtt_handle, message);
    free(message);
    message = NULL;

    EventBits_t bits = xEventGroupWaitBits(iot_chat->event_group_handle, ESP_IOT_CHAT_EVENT_SERVER_HELLO, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & ESP_IOT_CHAT_EVENT_SERVER_HELLO)) {
        ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Failed to receive server hello");
    }

    esp_err_t ret = esp_iot_chat_audio_open(iot_chat);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to open audio %s", esp_err_to_name(ret));

    esp_event_post(ESP_IOT_CHAT_EVENTS, ESP_IOT_CHAT_EVENT_AUDIO_CHANNEL_OPENED, NULL, 0, portMAX_DELAY);

    return ESP_OK;
}

esp_err_t esp_iot_chat_close_audio_channel(esp_iot_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;
    esp_iot_chat_audio_close(iot_chat);

    if (strlen(iot_chat->session_id) > 0) {
        cJSON *root = cJSON_CreateObject();
        if (root) {
            cJSON_AddStringToObject(root, "session_id", iot_chat->session_id);
            cJSON_AddStringToObject(root, "type", "goodbye");

            esp_iot_chat_send_json_message(chat_hd, root);
            cJSON_Delete(root);
        }
    }

    esp_event_post(ESP_IOT_CHAT_EVENTS, ESP_IOT_CHAT_EVENT_AUDIO_CHANNEL_CLOSED, NULL, 0, portMAX_DELAY);

    return ESP_OK;
}

esp_err_t esp_iot_chat_send_audio(esp_iot_chat_handle_t chat_hd, const esp_iot_chat_audio_packet_t *packet)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(packet, ESP_ERR_INVALID_ARG, TAG, "Invalid packet");
    ESP_RETURN_ON_FALSE(packet->payload, ESP_ERR_INVALID_ARG, TAG, "Invalid payload");
    ESP_RETURN_ON_FALSE(packet->payload_size > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid payload size");

    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;
    if (xSemaphoreTake(iot_chat->channel_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Failed to take mutex");
    }

    esp_err_t result = ESP_FAIL;
    if (esp_iot_chat_is_audio_ready(chat_hd)) {
        uint8_t nonce[ESP_IOT_MQTT_AES_NONCE_SIZE];
        memcpy(nonce, iot_chat->aes_nonce, ESP_IOT_MQTT_AES_NONCE_SIZE);

        *(uint16_t *)&nonce[2] = htons(packet->payload_size);
        *(uint32_t *)&nonce[8] = htonl(packet->timestamp);
        *(uint32_t *)&nonce[12] = htonl(++iot_chat->local_sequence);

        size_t encrypted_size = ESP_IOT_MQTT_AES_NONCE_SIZE + packet->payload_size;
        uint8_t *encrypted = malloc(encrypted_size);
        if (encrypted) {
            memcpy(encrypted, nonce, ESP_IOT_MQTT_AES_NONCE_SIZE);

            size_t nc_off = 0;
            uint8_t stream_block[16] = {0};

            int ret = mbedtls_aes_crypt_ctr(&iot_chat->aes_ctx, packet->payload_size, &nc_off, nonce, stream_block, packet->payload, &encrypted[ESP_IOT_MQTT_AES_NONCE_SIZE]);

            if (ret == 0) {
                ssize_t sent = send(iot_chat->udp_socket, (const char *)encrypted, encrypted_size, 0);
                result = (sent == (ssize_t)encrypted_size) ? ESP_OK : ESP_ERR_INVALID_ARG;
            } else {
                ESP_LOGE(TAG, "Failed to encrypt audio data: %d", ret);
            }

            free(encrypted);
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for encrypted audio data");
        }
    } else {
        ESP_LOGW(TAG, "Audio channel not opened");
    }

    xSemaphoreGive(iot_chat->channel_mutex);
    return result;
}

esp_err_t esp_iot_chat_send_audio_data(esp_iot_chat_handle_t chat_hd, const char *data, size_t data_len)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "Invalid data");

    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;
    esp_iot_chat_audio_packet_t packet = {
        .sample_rate = iot_chat->server_sample_rate,
        .frame_duration = iot_chat->server_frame_duration,
        .timestamp = xTaskGetTickCount(),
        .payload = (uint8_t *)data,
        .payload_size = data_len,
    };

    esp_iot_chat_send_audio(chat_hd, &packet);

    return ESP_OK;
}

esp_err_t esp_iot_chat_send_wake_word_detected(esp_iot_chat_handle_t chat_hd, const char *wake_word)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(wake_word, ESP_ERR_INVALID_ARG, TAG, "Invalid wake word");

    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "session_id", iot_chat->session_id);
    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "detect");
    cJSON_AddStringToObject(root, "text", wake_word);

    esp_err_t ret = esp_iot_chat_send_json_message(chat_hd, root);
    cJSON_Delete(root);

    return ret;
}

esp_err_t esp_iot_chat_send_start_listening(esp_iot_chat_handle_t chat_hd, esp_iot_chat_listening_mode_t mode)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_err_t ret = ESP_OK;
    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;
    cJSON *root = NULL;
    const char *mode_str = NULL;

    root = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(root, ESP_ERR_NO_MEM, TAG, "Failed to create JSON object");

    cJSON_AddStringToObject(root, "session_id", iot_chat->session_id);
    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "start");

    switch (mode) {
    case ESP_IOT_CHAT_LISTENING_MODE_REALTIME:
        mode_str = "realtime";
        break;
    case ESP_IOT_CHAT_LISTENING_MODE_AUTO:
        mode_str = "auto";
        break;
    case ESP_IOT_CHAT_LISTENING_MODE_MANUAL:
        mode_str = "manual";
        break;
    default:
        mode_str = "unknown";
        break;
    }
    cJSON_AddStringToObject(root, "mode", mode_str);

    ret = esp_iot_chat_send_json_message(chat_hd, root);
    cJSON_Delete(root);

    return ret;
}

esp_err_t esp_iot_chat_send_stop_listening(esp_iot_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_err_t ret = ESP_OK;
    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;
    cJSON *root = NULL;

    root = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(root, ESP_ERR_NO_MEM, TAG, "Failed to create JSON object");

    cJSON_AddStringToObject(root, "session_id", iot_chat->session_id);
    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "stop");

    ret = esp_iot_chat_send_json_message(chat_hd, root);
    cJSON_Delete(root);

    return ret;
}

esp_err_t esp_iot_chat_send_abort_speaking(esp_iot_chat_handle_t chat_hd, esp_iot_chat_abort_speaking_reason_t reason)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_err_t ret = ESP_OK;
    esp_iot_chat_t *iot_chat = (esp_iot_chat_t *)chat_hd;
    cJSON *root = NULL;
    const char *reason_str = NULL;

    root = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(root, ESP_ERR_NO_MEM, TAG, "Failed to create JSON object");

    cJSON_AddStringToObject(root, "session_id", iot_chat->session_id);
    cJSON_AddStringToObject(root, "type", "abort");

    switch (reason) {
    case ESP_IOT_CHAT_ABORT_SPEAKING_REASON_WAKE_WORD_DETECTED:
        reason_str = "wake_word_detected";
        break;
    case ESP_IOT_CHAT_ABORT_SPEAKING_REASON_STOP_LISTENING:
        reason_str = "stop_listening";
        break;
    default:
        reason_str = "unknown";
        break;
    }
    cJSON_AddStringToObject(root, "reason", reason_str);

    ret = esp_iot_chat_send_json_message(chat_hd, root);
    cJSON_Delete(root);

    return ret;
}
