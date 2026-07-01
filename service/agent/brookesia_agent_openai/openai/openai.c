
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_peer_default.h"
#include "esp_log.h"
#include <cJSON.h>

#include "openai.h"
#include "https_client.h"

#define TAG "OPENAI_APP"

#define SDP_MSG_SIZE 4096

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

static char *esp_peer_state_to_str(esp_peer_state_t state)
{
    switch (state) {
        case ESP_PEER_STATE_CLOSED:
            return "ESP_PEER_STATE_CLOSED";
        case ESP_PEER_STATE_DISCONNECTED:
            return "ESP_PEER_STATE_DISCONNECTED";
        case ESP_PEER_STATE_NEW_CONNECTION:
            return "ESP_PEER_STATE_NEW_CONNECTION";
        case ESP_PEER_STATE_PAIRING:
            return "ESP_PEER_STATE_PAIRING";
        case ESP_PEER_STATE_PAIRED:
            return "ESP_PEER_STATE_PAIRED";
        case ESP_PEER_STATE_CONNECTING:
            return "ESP_PEER_STATE_CONNECTING";
        case ESP_PEER_STATE_CONNECTED:
            return "ESP_PEER_STATE_CONNECTED";
        case ESP_PEER_STATE_CONNECT_FAILED:
            return "ESP_PEER_STATE_CONNECT_FAILED";
        case ESP_PEER_STATE_DATA_CHANNEL_CONNECTED:
            return "ESP_PEER_STATE_DATA_CHANNEL_CONNECTED";
        case ESP_PEER_STATE_DATA_CHANNEL_OPENED:
            return "ESP_PEER_STATE_DATA_CHANNEL_OPENED";
        case ESP_PEER_STATE_DATA_CHANNEL_CLOSED:
            return "ESP_PEER_STATE_DATA_CHANNEL_CLOSED";
        case ESP_PEER_STATE_DATA_CHANNEL_DISCONNECTED:
            return "ESP_PEER_STATE_DATA_CHANNEL_DISCONNECTED";
        default:
            return "UNKNOWN";
    }
}

static int peer_state_handler(esp_peer_state_t state, void *ctx)
{
    (void)ctx;
    ESP_LOGW(TAG, "Peer state: %s", esp_peer_state_to_str(state));
    if (state == ESP_PEER_STATE_DATA_CHANNEL_CONNECTED) {
        ESP_LOGI(TAG, "Peer data channel connected");
        xEventGroupSetBits(openai.peer_event_group, PEER_CONNECTED_BIT);
    }
    if (openai.config.audio_event_handler) {
        openai.config.audio_event_handler(state, NULL, openai.config.ctx);
    }
    return 0;
}

static int peer_msg_handler(esp_peer_msg_t *msg, void *ctx)
{
    (void)ctx;
    if (msg->type == ESP_PEER_MSG_TYPE_SDP) {
        ESP_LOGI(TAG, "Exchanging SDP with OpenAI Realtime GA API");
        esp_peer_msg_t sdp_msg = {
            .type = ESP_PEER_MSG_TYPE_SDP,
        };
        sdp_msg.data = (uint8_t *)calloc(1, SDP_MSG_SIZE);
        if (sdp_msg.data == NULL) {
            ESP_LOGE(TAG, "Fail to allocate memory for SDP message");
            return -1;
        }

        const char *model = openai.config.model ? openai.config.model : OPENAI_DEFAULT_MODEL;
        const char *voice = openai.config.voice ? openai.config.voice : OPENAI_DEFAULT_VOICE;

        cJSON *session = cJSON_CreateObject();
        cJSON_AddStringToObject(session, "type", "realtime");
        cJSON_AddStringToObject(session, "model", model);
        cJSON *audio = cJSON_CreateObject();
        cJSON *output = cJSON_CreateObject();
        cJSON_AddStringToObject(output, "voice", voice);
        cJSON_AddItemToObject(audio, "output", output);
        cJSON_AddItemToObject(session, "audio", audio);
        char *session_json = cJSON_PrintUnformatted(session);
        cJSON_Delete(session);
        if (session_json == NULL) {
            ESP_LOGE(TAG, "Fail to build session JSON");
            free(sdp_msg.data);
            return -1;
        }

        int status = https_post_realtime_call(openai.config.api_key, (char *)msg->data, session_json,
                                              (char *)sdp_msg.data);
        free(session_json);
        if (status < 200 || status >= 300 || sdp_msg.data[0] == '\0') {
            ESP_LOGE(TAG, "Realtime call failed, HTTP status=%d, body=%s", status, (char *)sdp_msg.data);
            free(sdp_msg.data);
            return -1;
        }

        sdp_msg.size = strlen((char *)sdp_msg.data);
        esp_peer_send_msg(openai.peer, &sdp_msg);
        free(sdp_msg.data);
    }
    return 0;
}


static int peer_audio_info_handler(esp_peer_audio_stream_info_t *info, void *ctx)
{
    (void)info;
    (void)ctx;
    return 0;
}

static int peer_audio_data_handler(esp_peer_audio_frame_t *frame, void *ctx)
{
    (void)ctx;
    if (openai.config.audio_data_handler) {
        openai.config.audio_data_handler(frame->data, frame->size, openai.config.ctx);
    }
    return 0;
}

static int peer_data_handler(esp_peer_data_frame_t *frame, void *ctx)
{
    (void)ctx;
    // ESP_LOGI(TAG, "Peer data handler: %s", (char *)frame->data);
    if (openai.config.audio_event_handler) {
        openai.config.audio_event_handler(ESP_PEER_MSG_EVENT, (void *)frame->data, openai.config.ctx);
    }

    return 0;
}

static void pc_task(void *arg)
{
    openai_t *openai = (openai_t *)arg;
    while (openai->peer_running) {
        esp_peer_main_loop(openai->peer);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    openai->peer_stopped = true;
    vTaskDelete(NULL);
}

esp_err_t openai_init(openai_config_t *config)
{
    if (config == NULL || config->api_key == NULL || config->api_key[0] == '\0') {
        ESP_LOGE(TAG, "OpenAI API key is empty. Set it in menuconfig: Example Audio Configuration -> OpenAI API Key");
        return ESP_ERR_INVALID_ARG;
    }

    esp_peer_default_cfg_t peer_cfg = {
        .agent_recv_timeout = 100,   // Enlarge this value if network is poor
        .data_ch_cfg = {
            .recv_cache_size = 1536, // Should big than one MTU size
            .send_cache_size = 1536, // Should big than one MTU size
        },
        .rtp_cfg = {
            .audio_recv_jitter = {
                .cache_size = 1024,
            },
            .send_pool_size = 1024,
            .send_queue_num = 10,
        },
    };
    esp_peer_cfg_t cfg = {
        .audio_dir = ESP_PEER_MEDIA_DIR_SEND_RECV,
        .audio_info = {
            .codec = ESP_PEER_AUDIO_CODEC_OPUS,
        },
        .enable_data_channel = true,
        .role = ESP_PEER_ROLE_CONTROLLING,
        .ice_trans_policy = ESP_PEER_ICE_TRANS_POLICY_ALL,
        .on_state = peer_state_handler,
        .on_msg = peer_msg_handler,
        .on_audio_info = peer_audio_info_handler,
        .on_audio_data = peer_audio_data_handler,
        .on_data = peer_data_handler,
        .extra_cfg = &peer_cfg,
        .extra_size = sizeof(esp_peer_default_cfg_t),
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
    if (openai.peer_event_group == NULL) {
        esp_peer_close(openai.peer);
        openai.peer = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t openai_start(void)
{
    openai.peer_running = true;
    openai.peer_stopped = false;
    // Run the WebRTC loop on a separate core because the handshake can be CPU heavy.
    if (xTaskCreatePinnedToCore(pc_task, "openai", 10 * 1024, &openai, 5, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "Fail to create thread %s", "openai");
        openai.peer_running = false;
        return ESP_FAIL;
    }
    int ret = esp_peer_new_connection(openai.peer);
    if (ret != ESP_PEER_ERR_NONE) {
        openai.peer_running = false;
        ESP_LOGE(TAG, "Fail to create peer connection ret %d", ret);
        return ESP_FAIL;
    }
    EventBits_t bits = xEventGroupWaitBits(openai.peer_event_group, PEER_CONNECTED_BIT, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(openai.config.connect_timeout_ms));
    if ((bits & PEER_CONNECTED_BIT) == 0) {
        ESP_LOGE(TAG, "Fail to connect to openai");
        return ESP_FAIL;
    }

    esp_peer_data_channel_cfg_t ch_cfg = {
        .type = ESP_PEER_DATA_CHANNEL_RELIABLE,
        .ordered = true,
        .label = "my_channel"
    };
    esp_peer_create_data_channel(openai.peer, &ch_cfg);
    return ESP_OK;
}

void openai_send_audio(uint8_t *data, size_t size)
{
    if (openai.peer == NULL || data == NULL || size == 0) {
        return;
    }
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
        openai.peer = NULL;
    }
    if (openai.peer_event_group) {
        vEventGroupDelete(openai.peer_event_group);
        openai.peer_event_group = NULL;
    }
    return ESP_OK;
}

esp_err_t openai_stop(void)
{
    if (openai.peer == NULL) {
        return ESP_OK;
    }
    esp_peer_disconnect(openai.peer);
    openai.peer_running = false;
    while (!openai.peer_stopped) {
        vTaskDelay(pdMS_TO_TICKS(100));
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
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    cJSON *item = cJSON_CreateObject();
    cJSON *content_array = cJSON_CreateArray();
    cJSON *content_item = cJSON_CreateObject();
    if (item == NULL || content_array == NULL || content_item == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(item);
        cJSON_Delete(content_array);
        cJSON_Delete(content_item);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "type", "conversation.item.create");
    cJSON_AddNullToObject(root, "previous_item_id");
    cJSON_AddStringToObject(item, "type", "message");
    cJSON_AddStringToObject(item, "role", "user");
    cJSON_AddStringToObject(content_item, "type", "input_text");
    cJSON_AddStringToObject(content_item, "text", text);
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(item, "content", content_array);
    cJSON_AddItemToObject(root, "item", item);

    char *send_text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (send_text == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "Send text event: %s", send_text);
    esp_peer_data_frame_t data_frame = {
        .type = ESP_PEER_DATA_CHANNEL_STRING,
        .data = (uint8_t *)send_text,
        .size = strlen(send_text),
    };
    int ret = esp_peer_send_data(openai.peer, &data_frame);
    free(send_text);
    return ret == ESP_PEER_ERR_NONE ? ESP_OK : ESP_FAIL;
}
