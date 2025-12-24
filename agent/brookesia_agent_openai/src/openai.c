/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_peer_default.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_random.h"
#include <cJSON.h>

#include "openai.h"
#include "openai_signaling.h"
#include "https_client.h"

#define TAG "OPENAI_APP"

#define OPENAI_REALTIME_URL     "https://api.openai.com/v1/realtime?model="

#define OPENAI_TASK_PRIORITY    10
#define OPENAI_TASK_CORE        1

typedef struct {
    esp_peer_handle_t  peer;
    EventGroupHandle_t  peer_event_group;
    bool               peer_running;
    bool               peer_connected;
    bool               peer_stopped;
    openai_config_t    config;
} openai_t;

static openai_t openai;

#define PEER_CONNECTED_BIT (1 << 0)

// static char *esp_peer_state_to_str(esp_peer_state_t state)
// {
//     switch (state) {
//     case ESP_PEER_STATE_CLOSED:
//         return "ESP_PEER_STATE_CLOSED";
//     case ESP_PEER_STATE_DISCONNECTED:
//         return "ESP_PEER_STATE_DISCONNECTED";
//     case ESP_PEER_STATE_NEW_CONNECTION:
//         return "ESP_PEER_STATE_NEW_CONNECTION";
//     case ESP_PEER_STATE_PAIRING:
//         return "ESP_PEER_STATE_PAIRING";
//     case ESP_PEER_STATE_PAIRED:
//         return "ESP_PEER_STATE_PAIRED";
//     case ESP_PEER_STATE_CONNECTING:
//         return "ESP_PEER_STATE_CONNECTING";
//     case ESP_PEER_STATE_CONNECTED:
//         return "ESP_PEER_STATE_CONNECTED";
//     case ESP_PEER_STATE_CONNECT_FAILED:
//         return "ESP_PEER_STATE_CONNECT_FAILED";
//     case ESP_PEER_STATE_DATA_CHANNEL_CONNECTED:
//         return "ESP_PEER_STATE_DATA_CHANNEL_CONNECTED";
//     case ESP_PEER_STATE_DATA_CHANNEL_OPENED:
//         return "ESP_PEER_STATE_DATA_CHANNEL_OPENED";
//     case ESP_PEER_STATE_DATA_CHANNEL_CLOSED:
//         return "ESP_PEER_STATE_DATA_CHANNEL_CLOSED";
//     case ESP_PEER_STATE_DATA_CHANNEL_DISCONNECTED:
//         return "ESP_PEER_STATE_DATA_CHANNEL_DISCONNECTED";
//     default:
//         return "UNKNOWN";
//     }
// }

static int peer_state_handler(esp_peer_state_t state, void *ctx)
{
    if (state == ESP_PEER_STATE_DATA_CHANNEL_CONNECTED) {
        xEventGroupSetBits(openai.peer_event_group, PEER_CONNECTED_BIT);
    }
    if (openai.config.audio_event_handler) {
        openai.config.audio_event_handler(state, NULL, openai.config.ctx);
    }
    return 0;
}

static int peer_msg_handler(esp_peer_msg_t *msg, void *ctx)
{
    if (msg->type == ESP_PEER_MSG_TYPE_SDP) {
        printf("SDP: %s\n", (char *)msg->data);
        // Exchange SDP with peer
#define SDP_MSG_SIZE 4096
        esp_peer_msg_t sdp_msg = {
            .type = ESP_PEER_MSG_TYPE_SDP,
        };
        sdp_msg.data = (uint8_t *)calloc(1, SDP_MSG_SIZE);
        if (sdp_msg.data == NULL) {
            ESP_LOGE(TAG, "Fail to allocate memory for SDP message");
            return -1;
        }
        char url[256] = { 0 };
        sprintf(url, OPENAI_REALTIME_URL "%s", openai.config.model);
        https_post(url, openai.config.api_key, (char *)msg->data, (char *)sdp_msg.data);

        sdp_msg.size = strlen((char *)sdp_msg.data);
        esp_peer_send_msg(openai.peer, &sdp_msg);

        free(sdp_msg.data);
    }
    return 0;
}


static int peer_audio_info_handler(esp_peer_audio_stream_info_t *info, void *ctx)
{
    return 0;
}

static int peer_audio_data_handler(esp_peer_audio_frame_t *frame, void *ctx)
{
    if (openai.config.audio_data_handler) {
        openai.config.audio_data_handler(frame->data, frame->size, openai.config.ctx);
    }
    return 0;
}


static int peer_data_handler(esp_peer_data_frame_t *frame, void *ctx)
{
    // ESP_LOGI(TAG, "Peer data handler: %s", (char *)frame->data);
    if (openai.config.audio_event_handler) {
        openai.config.audio_event_handler(ESP_PEER_MSG_EVENT, frame->data, openai.config.ctx);
    }

    return 0;
}

static void pc_task(void *arg)
{
    openai_t *openai = (openai_t *)arg;

    openai->peer_stopped = false;
    while (openai->peer_running) {
        esp_peer_main_loop(openai->peer);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    openai->peer_stopped = true;
    vTaskDelete(NULL);
}

esp_err_t openai_init(openai_config_t *config)
{
    // esp_peer_default_cfg_t peer_cfg = {
    //     .agent_recv_timeout = 100,   // Enlarge this value if network is poor
    //     .data_ch_cfg = {
    //         .recv_cache_size = 1536, // Should big than one MTU size
    //         .send_cache_size = 1536, // Should big than one MTU size
    //     },
    //     .rtp_cfg = {
    //         .audio_recv_jitter = {
    //             .cache_size = 1024,
    //         },
    //         .send_pool_size = 1024,
    //         .send_queue_num = 10,
    //     },
    // };
    esp_peer_cfg_t cfg = {
        //.server_lists = &server_info, // Should set to actual stun/turn servers
        //.server_num = 0,
        .audio_dir = ESP_PEER_MEDIA_DIR_SEND_RECV,
        .audio_info = {
            .codec = ESP_PEER_AUDIO_CODEC_OPUS,
        },
        .enable_data_channel = true,
        .role = ESP_PEER_ROLE_CONTROLLING,
        .on_state = peer_state_handler,
        .on_msg = peer_msg_handler,
        .on_audio_info = peer_audio_info_handler,
        .on_audio_data = peer_audio_data_handler,
        .on_data = peer_data_handler,
        .ctx = config->ctx,
        // .extra_cfg = &peer_cfg,
        // .extra_size = sizeof(esp_peer_default_cfg_t),
    };
    int ret = esp_peer_open(&cfg, esp_peer_get_default_impl(), &openai.peer);
    if (ret != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG, "Fail to create PeerConnection ret %d", ret);
        return ret;
    }
    if (config->connect_timeout_ms == 0) {
        config->connect_timeout_ms = OPENAI_DEFAULT_CONNECT_TIMEOUT_MS;
    }
    openai.config = *config;
    openai.peer_event_group = xEventGroupCreate();

    return ESP_OK;
}

esp_err_t openai_start(void)
{
    if (openai.peer_running) {
        return ESP_OK;
    }

    openai.peer_running = true;

    // We create on separate core to avoid when handshake (heavy calculation) peer Cat block peer Sheep
    if (xTaskCreatePinnedToCore(pc_task, "openai", 10 * 1024, &openai, OPENAI_TASK_PRIORITY, NULL, OPENAI_TASK_CORE) != pdPASS) {
        ESP_LOGE(TAG, "Fail to create thread %s", "openai");
        return ESP_FAIL;
    }
    // Create connection
    esp_err_t ret = ESP_OK;
    int result = esp_peer_new_connection(openai.peer);
    if (result != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG, "Fail to create connection ret %d", result);
        goto err;
    }

    ESP_LOGI(TAG, "Openai wait for connected");
    xEventGroupWaitBits(openai.peer_event_group, PEER_CONNECTED_BIT, pdFALSE, pdFALSE, openai.config.connect_timeout_ms);
    if (!(xEventGroupGetBits(openai.peer_event_group) & PEER_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "Openai connected timeout");
        ret = ESP_ERR_TIMEOUT;
        goto err;
    }
    xEventGroupClearBits(openai.peer_event_group, PEER_CONNECTED_BIT);

    {
        esp_peer_data_channel_cfg_t ch_cfg = {
            .type = ESP_PEER_DATA_CHANNEL_RELIABLE,
            .ordered = true,
            .label = "my_channel"
        };
        ret = esp_peer_create_data_channel(openai.peer, &ch_cfg);
        if (ret != ESP_PEER_ERR_NONE) {
            ESP_LOGE(TAG, "Fail to create data channel ret %d", ret);
            ret = ESP_FAIL;
            goto err;
        }
    }

    ESP_LOGI(TAG, "Openai started");

    return ESP_OK;

err:
    openai_stop();

    return ret;
}

void openai_send_audio(uint8_t *data, size_t size)
{
    esp_peer_audio_frame_t audio_frame = {
        .data = data,
        .size = size,
    };
    esp_peer_send_audio(openai.peer, &audio_frame);
}

esp_err_t openai_deinit(void)
{
    if (openai.peer) {
        esp_peer_close(openai.peer);
    }
    return ESP_OK;
}

esp_err_t openai_stop(void)
{
    if (!openai.peer_running) {
        return ESP_OK;
    }

    esp_peer_disconnect(openai.peer);
    openai.peer_running = false;
    while (!openai.peer_stopped) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_OK;
}

esp_err_t openai_send_text(const char *text)
{
    if (openai.peer == NULL) {
        ESP_LOGE(TAG, "WebRTC not started yet");
        return ESP_FAIL;
    }
    cJSON *root = cJSON_CreateObject();
    if (root) {
        cJSON_AddStringToObject(root, "type", "conversation.item.create");
        cJSON_AddNullToObject(root, "previous_item_id");
        cJSON *item = cJSON_CreateObject();
        if (item) {
            cJSON_AddStringToObject(item, "type", "message");
            cJSON_AddStringToObject(item, "role", "user");
        }
        cJSON *contentArray = cJSON_CreateArray();
        cJSON *contentItem = cJSON_CreateObject();
        cJSON_AddStringToObject(contentItem, "type", "input_text");
        cJSON_AddStringToObject(contentItem, "text", text);
        cJSON_AddItemToArray(contentArray, contentItem);
        cJSON_AddItemToObject(item, "content", contentArray);
        // Add the item to the root object
        cJSON_AddItemToObject(root, "item", item);
    }
    // Print the initial JSON structure
    char *send_text = cJSON_Print(root);
    if (send_text) {
        printf("Begin to send json:%s\n", send_text);
        esp_peer_data_frame_t data_frame = {
            .type = ESP_PEER_DATA_CHANNEL_STRING,
            .data = (uint8_t *)send_text,
            .size = strlen(send_text),
        };
        return esp_peer_send_data(openai.peer, &data_frame);
        // Clean up
        free(send_text);
    }
    cJSON_Delete(root); // Free the cJSON object
    return ESP_OK;
}
