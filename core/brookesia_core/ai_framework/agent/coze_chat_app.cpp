/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mutex>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_gmf_element.h"
#include "esp_gmf_oal_sys.h"
#include "esp_gmf_oal_thread.h"
#include "esp_gmf_oal_mem.h"
#include "esp_coze_chat.h"
#include "esp_coze_utils.h"
#include "http_client_request.h"
#include "cJSON.h"
#include "boost/thread.hpp"
#include "private/esp_brookesia_ai_agent_utils.hpp"
#include "audio_processor.h"
#include "function_calling.hpp"
#include "coze_chat_app.hpp"

#define SPEAKING_TIMEOUT_MS         (2000)
#define SPEAKING_MUTE_DELAY_MS      (2000)

#define AUDIO_RECORDER_READ_SIZE    (1024)

#define COZE_INTERRUPT_TIMES        (20)
#define COZE_INTERRUPT_INTERVAL_MS  (100)

using namespace esp_brookesia::ai_framework;

struct coze_chat_t {
    esp_coze_chat_handle_t  chat;
    std::recursive_mutex    chat_mutex;
    bool                    chat_start;
    bool                    chat_pause;
    bool                    chat_sleep;
    bool                    speaking;
    bool                    wakeup;
    bool                    wakeup_start;
    bool                    websocket_connected;
    esp_timer_handle_t      speaking_timeout_timer;
    esp_gmf_oal_thread_t    read_thread;
    esp_gmf_oal_thread_t    btn_thread;
    QueueHandle_t           btn_evt_q;
};

static struct coze_chat_t coze_chat = {};
static const char *coze_authorization_url = "https://api.coze.cn/api/permission/oauth2/token";

boost::signals2::signal<void(const std::string &emoji)> coze_chat_emoji_signal;
boost::signals2::signal<void(bool is_speaking)> coze_chat_speaking_signal;
boost::signals2::signal<void(void)> coze_chat_response_signal;
boost::signals2::signal<void(bool is_wake_up)> coze_chat_wake_up_signal;
boost::signals2::signal<void(void)> coze_chat_websocket_disconnected_signal;
boost::signals2::signal<void(int code)> coze_chat_error_signal;

void CozeChatAgentInfo::dump() const
{
    ESP_UTILS_LOGI(
        "\n{ChatInfo}:\n"
        "\t-session_name: %s\n"
        "\t-device_id: %s\n"
        "\t-app_id: %s\n"
        "\t-user_id: %s\n"
        "\t-public_key: %s\n"
        "\t-private_key: %s\n"
        "\t-custom_consumer: %s\n",
        session_name.c_str(), device_id.c_str(), app_id.c_str(), user_id.c_str(), public_key.c_str(),
        private_key.c_str(), custom_consumer.c_str()
    );
}

bool CozeChatAgentInfo::isValid() const
{
    return !session_name.empty() && !device_id.empty() && !user_id.empty() && !app_id.empty() &&
           !public_key.empty() && !private_key.empty();
}

void CozeChatRobotInfo::dump() const
{
    ESP_UTILS_LOGI(
        "\n{RobotInfo}:\n"
        "\t-name: %s\n"
        "\t-bot_id: %s\n"
        "\t-voice_id: %s\n"
        "\t-description: %s\n",
        name.c_str(), bot_id.c_str(), voice_id.c_str(), description.c_str()
    );
}

bool CozeChatRobotInfo::isValid() const
{
    return !name.empty() && !bot_id.empty() && !voice_id.empty() && !description.empty();
}


static void change_speaking_state(bool is_speaking, bool force = false)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    esp_err_t ret = ESP_OK;
    // std::unique_lock<std::recursive_mutex> lock(coze_chat.chat_mutex);

    if ((is_speaking == coze_chat.speaking) && !force) {
        if (is_speaking) {
            ret = esp_timer_restart(coze_chat.speaking_timeout_timer, SPEAKING_TIMEOUT_MS * 1000);
            if (ret != ESP_OK) {
                ESP_UTILS_LOGE("Restart speaking timeout timer failed(%s)", esp_err_to_name(ret));
            }
        }
        return;
    }

    ESP_UTILS_LOGI("change_speaking_state: %d, force: %d", is_speaking, force);

    if (is_speaking) {
        if (esp_gmf_afe_keep_awake(audio_processor_get_afe_handle(), true) != ESP_OK) {
            ESP_UTILS_LOGE("Keep awake failed");
        }
        if (!esp_timer_is_active(coze_chat.speaking_timeout_timer)) {
            ret = esp_timer_start_once(coze_chat.speaking_timeout_timer, SPEAKING_TIMEOUT_MS * 1000);
            if (ret != ESP_OK) {
                ESP_UTILS_LOGE("Start speaking timeout timer failed(%s)", esp_err_to_name(ret));
            }
        }
    } else {
        if (esp_gmf_afe_keep_awake(audio_processor_get_afe_handle(), false) != ESP_OK) {
            ESP_UTILS_LOGE("Keep awake failed");
        }
        if (esp_timer_is_active(coze_chat.speaking_timeout_timer)) {
            ret = esp_timer_stop(coze_chat.speaking_timeout_timer);
            if (ret != ESP_OK) {
                ESP_UTILS_LOGE("Stop speaking timeout timer failed(%s)", esp_err_to_name(ret));
            }
        }
    }
    coze_chat.speaking = is_speaking;
    // lock.unlock();

    coze_chat_speaking_signal(is_speaking);
}

static void change_wakeup_state(bool is_wakeup, bool force = false)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    // std::unique_lock<std::recursive_mutex> lock(coze_chat.chat_mutex);

    if ((is_wakeup == coze_chat.wakeup) && !force) {
        return;
    }

    ESP_UTILS_LOGI("change_wakeup_state: %d, force: %d", is_wakeup, force);

    coze_chat.wakeup = is_wakeup;
    // lock.unlock();

    coze_chat_wake_up_signal(is_wakeup);
}

static int parse_chat_error_code(const char *data)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_NULL_RETURN(data, -1, "Invalid data");

    cJSON *json_root = cJSON_Parse(data);
    ESP_UTILS_CHECK_NULL_RETURN(json_root, -1, "Failed to parse JSON data");

    esp_utils::function_guard delete_guard([&]() {
        cJSON_Delete(json_root);
    });

    cJSON *data_obj = cJSON_GetObjectItem(json_root, "data");
    ESP_UTILS_CHECK_NULL_RETURN(data_obj, -1, "No data found in JSON data");
    ESP_UTILS_CHECK_FALSE_RETURN(cJSON_IsObject(data_obj), -1, "data is not an object");

    cJSON *code_item = cJSON_GetObjectItem(data_obj, "code");
    ESP_UTILS_CHECK_NULL_RETURN(code_item, -1, "No code found in JSON data");
    ESP_UTILS_CHECK_FALSE_RETURN(cJSON_IsNumber(code_item), -1, "code is not a number");

    return static_cast<int>(cJSON_GetNumberValue(code_item));
}

static void audio_event_callback(esp_coze_chat_event_t event, char *data, void *ctx)
{
    if (event == ESP_COZE_CHAT_EVENT_CHAT_ERROR) {
        ESP_UTILS_LOGE("chat error: %s", data);

        int code = parse_chat_error_code(data);
        ESP_UTILS_CHECK_FALSE_EXIT(code != -1, "Failed to parse chat error code");

        coze_chat_error_signal(code);
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STARTED) {
        ESP_UTILS_LOGI("chat start");
        coze_chat.wakeup_start = false;
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STOPED) {
        ESP_UTILS_LOGI("chat stop");
        // change_speaking_state(true);
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_COMPLETED) {
        boost::thread([&]() {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(SPEAKING_MUTE_DELAY_MS));
            change_speaking_state(false);
        }).detach();
        ESP_UTILS_LOGI("chat complete");
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA) {
        // cjson format data
        ESP_UTILS_LOGI("Customer data: %s", data);

        // call func
        cJSON *json_data = cJSON_Parse(data);
        if (json_data == NULL) {
            ESP_UTILS_LOGE("Failed to parse JSON data");
            return;
        }
        // debug
        cJSON *json_item = NULL;
        cJSON_ArrayForEach(json_item, json_data) {
            char *key = json_item->string;
            char *value = cJSON_Print(json_item);
            if (key && value) {
                ESP_UTILS_LOGI("Key: %s, Value: %s", key, value);
                cJSON_free(value);
            }
        }
        //

        cJSON *data_json = cJSON_GetObjectItem(json_data, "data");
        if (data_json == NULL) {
            ESP_UTILS_LOGE("No data found in JSON data");
            cJSON_Delete(json_data);
            return;
        }


        cJSON *required_action = cJSON_GetObjectItem(data_json, "required_action");
        if (required_action == NULL) {
            ESP_UTILS_LOGE("No required_action found in JSON data");
            cJSON_Delete(json_data);
            return;
        }

        cJSON *submit_tool_outputs = cJSON_GetObjectItem(required_action, "submit_tool_outputs");
        if (submit_tool_outputs == NULL) {
            ESP_UTILS_LOGE("No submit_tool_outputs found in JSON data");
            cJSON_Delete(json_data);
            return;
        }

        cJSON *tool_calls = cJSON_GetObjectItem(submit_tool_outputs, "tool_calls");
        if (tool_calls == NULL || !cJSON_IsArray(tool_calls)) {
            ESP_UTILS_LOGE("No tool_calls found or tool_calls is not an array");
            cJSON_Delete(json_data);
            return;
        }

        cJSON *first_tool_call = cJSON_GetArrayItem(tool_calls, 0);
        if (first_tool_call == NULL) {
            ESP_UTILS_LOGE("No first tool call found in tool_calls");
            cJSON_Delete(json_data);
            return;
        }

        char *function_str = cJSON_Print(first_tool_call);
        if (function_str) {
            ESP_UTILS_LOGI("Function JSON: %s", function_str);
            free(function_str);
        } else {
            ESP_UTILS_LOGE("Failed to print function JSON");
        }

        FunctionDefinitionList::requestInstance().invokeFunction(first_tool_call);

        cJSON_Delete(json_data);
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT) {
        if (strncmp(data, "（", 3) == 0 && strncmp(data + strlen(data) - 3, "）", 3) == 0) {
            std::string emoji_str(data + 3);
            emoji_str = emoji_str.substr(0, emoji_str.length() - 3);
            if (emoji_str.front() == ':' && emoji_str.back() == ':') {
                emoji_str = emoji_str.substr(1, emoji_str.length() - 2);
                ESP_UTILS_LOGI("Emoji: %s\n", emoji_str.c_str());
                coze_chat_emoji_signal(emoji_str);
            }
        }
    }
}

static void websocket_event_callback(esp_coze_ws_event_t *event)
{
    switch (event->event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_UTILS_LOGI("Websocket connected");
        coze_chat.websocket_connected = true;
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_ERROR:
        ESP_UTILS_LOGE("Websocke_signalt disconnected or error");
        coze_chat.websocket_connected = false;
        coze_chat_websocket_disconnected_signal();
        break;
    default:
        break;
    }
}

static void audio_data_callback(char *data, int len, void *ctx)
{
    ESP_UTILS_LOGD("audio_data_callback");
    if (!coze_chat.chat_pause && !coze_chat.chat_sleep && coze_chat.speaking) {
        audio_playback_feed_data((uint8_t *)data, len);
    }
    if (!coze_chat.wakeup_start && !coze_chat.chat_pause && !coze_chat.chat_sleep) {
        change_speaking_state(true);
    }
}

void generate_random_string(char *output, size_t length)
{
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    size_t charset_size = sizeof(charset) - 1;
    for (size_t i = 0; i < length; i++) {
        int key = esp_random() % charset_size;
        output[i] = charset[key];
    }
    output[length] = '\0';
}

static char *coze_get_access_token(const CozeChatAgentInfo &agent_info)
{
    // 构建 JWT payload
    cJSON *payload_json = cJSON_CreateObject();
    if (!payload_json) {
        ESP_UTILS_LOGE("Failed to create payload_json");
        return NULL;
    }
    char random_str[33] = {0};
    generate_random_string(random_str, 32);
    time_t now = time(NULL);
    cJSON_AddStringToObject(payload_json, "iss", agent_info.app_id.c_str());
    cJSON_AddStringToObject(payload_json, "aud", "api.coze.cn");
    cJSON_AddNumberToObject(payload_json, "iat", now);
    cJSON_AddNumberToObject(payload_json, "exp", now + 6000);
    cJSON_AddStringToObject(payload_json, "jti", random_str);
    cJSON_AddStringToObject(payload_json, "session_name", agent_info.session_name.c_str());
    cJSON *session_context_json = cJSON_CreateObject();
    cJSON *device_info_json = cJSON_CreateObject();
    cJSON_AddStringToObject(device_info_json, "device_id", agent_info.device_id.c_str());
    cJSON_AddStringToObject(device_info_json, "custom_consumer", agent_info.custom_consumer.c_str());
    cJSON_AddItemToObject(session_context_json, "device_info", device_info_json);
    cJSON_AddItemToObject(payload_json, "session_context", session_context_json);

    char *payload_str = cJSON_PrintUnformatted(payload_json);
    if (!payload_str) {
        ESP_UTILS_LOGE("Failed to print payload_json");
        cJSON_Delete(payload_json);
        return NULL;
    }
    ESP_UTILS_LOGD("payload_str: %s\n", payload_str);
    char *formatted_payload_str = cJSON_Print(payload_json);
    if (formatted_payload_str) {
        ESP_UTILS_LOGD("formatted_payload_str: %s\n", formatted_payload_str);
        free(formatted_payload_str);
    }

    char *jwt = coze_jwt_create_handler(
                    agent_info.public_key.c_str(), payload_str, (const uint8_t *)agent_info.private_key.c_str(),
                    strlen(agent_info.private_key.c_str())
                );
    cJSON_Delete(payload_json);
    free(payload_str);

    if (!jwt) {
        ESP_UTILS_LOGE("Failed to create JWT");
        // payload_json and payload_str already freed above
        return NULL;
    }

    char *authorization = (char *)calloc(1, strlen(jwt) + 16);
    if (!authorization) {
        ESP_UTILS_LOGE("Failed to allocate authorization");
        free(jwt);
        return NULL;
    }
    sprintf(authorization, "Bearer %s", jwt);
    ESP_UTILS_LOGD("Authorization: %s", authorization);

    cJSON *http_req_json = cJSON_CreateObject();
    if (!http_req_json) {
        ESP_UTILS_LOGE("Failed to create http_req_json");
        free(jwt);
        free(authorization);
        return NULL;
    }
    cJSON_AddNumberToObject(http_req_json, "duration_seconds", 86399);
    cJSON_AddStringToObject(http_req_json, "grant_type", "urn:ietf:params:oauth:grant-type:jwt-bearer");
    char *http_req_json_str = cJSON_PrintUnformatted(http_req_json);
    if (!http_req_json_str) {
        ESP_UTILS_LOGE("Failed to print http_req_json");
        free(jwt);
        free(authorization);
        cJSON_Delete(http_req_json);
        return NULL;
    }

    http_req_header_t header[] = {
        {"Content-Type", "application/json"},
        {"Authorization", authorization},
        {NULL, NULL}
    };

    http_response_t response = {0};
    esp_err_t ret = http_client_post(coze_authorization_url, header, http_req_json_str, &response);
    if (ret != ESP_OK) {
        ESP_UTILS_LOGE("HTTP POST failed");
        return NULL;
    }

    char *access_token = NULL;
    if (response.body) {
        ESP_UTILS_LOGD("response: %s\n", response.body);

        cJSON *root = cJSON_Parse(response.body);
        if (root) {
            cJSON *access_token_item = cJSON_GetObjectItem(root, "access_token");
            if (cJSON_IsString(access_token_item) && access_token_item->valuestring != NULL) {
                ESP_UTILS_LOGD("access_token: %s\n", access_token_item->valuestring);
                access_token = strdup(access_token_item->valuestring);
            } else {
                ESP_UTILS_LOGE("access_token is invalid or not exist");
            }

            cJSON *expires_in_item = cJSON_GetObjectItem(root, "expires_in");
            if (cJSON_IsNumber(expires_in_item)) {
                ESP_UTILS_LOGD("expires_in: %d\n", expires_in_item->valueint);
            }

            cJSON *token_type_item = cJSON_GetObjectItem(root, "token_type");
            if (cJSON_IsString(token_type_item)) {
                ESP_UTILS_LOGD("token_type: %s\n", token_type_item->valuestring);
            }

            cJSON_Delete(root);
        } else {
            ESP_UTILS_LOGE("Failed to parse JSON response");
        }
    }

    // 释放内存
    free(jwt);
    free(authorization);
    cJSON_Delete(http_req_json);
    free(http_req_json_str);
    if (response.body) {
        free(response.body);
    }

    return access_token;
}

static void recorder_event_callback_fn(void *event, void *ctx)
{
    if (!coze_chat.chat_start || coze_chat.chat_pause) {
        ESP_UTILS_LOGD("chat is not started or paused, skip AFE event");
        return;
    }

    esp_gmf_afe_evt_t *afe_evt = (esp_gmf_afe_evt_t *)event;
    switch (afe_evt->type) {
    case ESP_GMF_AFE_EVT_WAKEUP_START: {
        ESP_UTILS_LOGI("wakeup start");
        if (coze_chat.websocket_connected && !coze_chat.chat_sleep) {
            coze_chat_app_interrupt();
        }
        change_speaking_state(false);
        change_wakeup_state(true);
        coze_chat.wakeup_start = true;
        coze_chat_response_signal();
        break;
    }
    case ESP_GMF_AFE_EVT_WAKEUP_END:
        ESP_UTILS_LOGI("wakeup end");
        change_speaking_state(false);
        change_wakeup_state(false);
        break;
    case ESP_GMF_AFE_EVT_VAD_START:
        ESP_UTILS_LOGI("vad start");
        break;
    case ESP_GMF_AFE_EVT_VAD_END:
        ESP_UTILS_LOGI("vad end");
        break;
    case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT:
        ESP_UTILS_LOGI("vcmd detect timeout");
        break;
    default: {
        // TODO: vcmd detected
        // esp_gmf_afe_vcmd_info_t *info = event->event_data;
        // ESP_UTILS_LOGW("Command %d, phrase_id %d, prob %f, str: %s", sevent->type, info->phrase_id, info->prob, info->str);
    }
    }
}

static void audio_data_read_task(void *pv)
{
    coze_chat_t *coze_chat = (coze_chat_t *)pv;

    uint8_t *data = (uint8_t *)esp_gmf_oal_calloc(1, AUDIO_RECORDER_READ_SIZE);
    int ret = 0;
    while (true) {
        ret = audio_recorder_read_data(data, AUDIO_RECORDER_READ_SIZE);
        if (coze_chat->chat_start && coze_chat->wakeup && !coze_chat->chat_pause && !coze_chat->chat_sleep && !coze_chat->speaking) {
            esp_coze_chat_send_audio_data(coze_chat->chat, (char *)data, ret);
        }
        // heap_caps_check_integrity_all(true);
    }
}

static void audio_pipe_open(void)
{
    vTaskDelay(pdMS_TO_TICKS(800)); // Delay a little time to stagger other initializations
    audio_recorder_open(recorder_event_callback_fn, NULL);
    audio_playback_open();
    audio_playback_run();
}

// static void audio_pipe_close(void)
// {
//     audio_playback_stop();
//     audio_playback_close();
//     audio_recorder_close();
// }

esp_err_t coze_chat_app_init(void)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    esp_timer_create_args_t timer_args = {
        .callback = [](void *arg)
        {
            ESP_UTILS_LOGI("speaking timeout start");
            boost::thread([&]() {
                change_speaking_state(false);
            }).detach();
            ESP_UTILS_LOGI("speaking timeout end");
        },
        .arg = &coze_chat,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "speaking_timeout",
        .skip_unhandled_events = false
    };
    esp_timer_create(&timer_args, &coze_chat.speaking_timeout_timer);

    audio_pipe_open();

    esp_gmf_oal_thread_create(
        &coze_chat.read_thread, "audio_data_read", audio_data_read_task, (void *)&coze_chat, 3096, 12, true, 1
    );

    return ESP_OK;
}

esp_err_t coze_chat_app_start(const CozeChatAgentInfo &agent_info, const CozeChatRobotInfo &robot_info)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    char *token_str = NULL;
    token_str = coze_get_access_token(agent_info);
    if (token_str == NULL) {
        ESP_UTILS_LOGE("Failed to get access token");
        return ESP_FAIL;
    }

    esp_coze_chat_config_t chat_config = ESP_COZE_CHAT_DEFAULT_CONFIG();
    chat_config.enable_subtitle = true;
    chat_config.subscribe_event = (const char *[]) {
        "conversation.chat.requires_action", NULL
    };
    chat_config.user_id = const_cast<char *>(agent_info.user_id.c_str());
    chat_config.bot_id = const_cast<char *>(robot_info.bot_id.c_str());
    chat_config.voice_id = const_cast<char *>(robot_info.voice_id.c_str());
    chat_config.access_token = token_str;
    chat_config.uplink_audio_type = ESP_COZE_CHAT_AUDIO_TYPE_G711A;
    chat_config.audio_callback = audio_data_callback;
    chat_config.event_callback = audio_event_callback;
    chat_config.ws_event_callback = websocket_event_callback;
    // chat_config.websocket_buffer_size = 4096;
    // chat_config.mode = ESP_COZE_CHAT_NORMAL_MODE;

    std::lock_guard lock(coze_chat.chat_mutex);
    esp_err_t ret = esp_coze_chat_init(&chat_config, &coze_chat.chat);
    ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, ret, "esp_coze_chat_init failed(%s)", esp_err_to_name(ret));

    static auto func_call = FunctionDefinitionList::requestInstance().getJson();

    esp_coze_parameters_kv_t param[] = {
        {"func_call", const_cast<char *>(func_call.c_str())},
        {NULL, NULL}
    };
    ret = esp_coze_set_chat_config_parameters(coze_chat.chat, param);
    ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, ret, "esp_coze_set_chat_config_parameters failed(%s)", esp_err_to_name(ret));
    ret = esp_coze_chat_start(coze_chat.chat);
    ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, ret, "esp_coze_chat_start failed(%s)", esp_err_to_name(ret));

    coze_chat.chat_start = true;

    free(token_str);

    return ESP_OK;
}

esp_err_t coze_chat_app_stop(void)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    std::lock_guard lock(coze_chat.chat_mutex);

    esp_err_t ret = esp_coze_chat_stop(coze_chat.chat);
    ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, ret, "esp_coze_chat_stop failed(%s)", esp_err_to_name(ret));

    ret = esp_coze_chat_deinit(coze_chat.chat);
    ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, ret, "esp_coze_chat_deinit failed(%s)", esp_err_to_name(ret));
    coze_chat.chat = NULL;

    coze_chat.chat_start = false;

    return ESP_OK;
}

void coze_chat_app_resume(void)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    coze_chat.chat_pause = false;
}

void coze_chat_app_pause(void)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    if (coze_chat.websocket_connected) {
        coze_chat_app_interrupt();
    }
    // esp_gmf_afe_reset_state(audio_processor_get_afe_handle());
    coze_chat.chat_pause = true;
    change_speaking_state(false);
    // change_wakeup_state(false);
}

void coze_chat_app_wakeup(void)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    coze_chat.chat_sleep = false;
    change_wakeup_state(true);
}

void coze_chat_app_sleep(void)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    if (coze_chat.websocket_connected) {
        coze_chat_app_interrupt();
    }
    coze_chat.chat_sleep = true;
    change_wakeup_state(false);
    change_speaking_state(false);
}

void coze_chat_app_interrupt(void)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    boost::thread([&]() {
        ESP_UTILS_LOG_TRACE_GUARD();
        for (int i = 0; i < COZE_INTERRUPT_TIMES; i++) {
            {
                std::lock_guard lock(coze_chat.chat_mutex);
                if ((coze_chat.chat == NULL) || !coze_chat.websocket_connected) {
                    break;
                }
                esp_coze_chat_send_audio_cancel(coze_chat.chat);
            }
            boost::this_thread::sleep_for(boost::chrono::milliseconds(COZE_INTERRUPT_INTERVAL_MS));
        }
    }).detach();
}
