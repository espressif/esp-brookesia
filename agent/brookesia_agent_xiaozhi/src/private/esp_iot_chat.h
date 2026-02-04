/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_heap_caps.h"
#include "esp_err.h"
#include "esp_event_base.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Audio packet for IOT chat
 */
typedef struct {
    int sample_rate;                                /*!< Sample rate */
    int frame_duration;                             /*!< Frame duration */
    uint32_t timestamp;                             /*!< Timestamp */
    uint8_t *payload;                               /*!< Payload */
    size_t payload_size;                            /*!< Payload size */
} esp_iot_chat_audio_packet_t;

/**
 * @brief  Information for IoT chat
 */
typedef struct {
    char *current_version;          /*!< Current version of the firmware */
    char *firmware_version;         /*!< Firmware version */
    char *firmware_url;             /*!< Firmware URL */
    char *serial_number;            /*!< Serial number */
    char *activation_code;          /*!< Activation code */
    char *activation_challenge;     /*!< Activation challenge */
    char *activation_message;       /*!< Activation message */
    int activation_timeout_ms;      /*!< Activation timeout in milliseconds */
    bool has_serial_number;         /*!< Has serial number */
    bool has_new_version;           /*!< Has new version */
    bool has_activation_code;       /*!< Has activation code */
    bool has_activation_challenge;  /*!< Has activation challenge */
    bool has_mqtt_config;           /*!< Has MQTT config */
    bool has_websocket_config;      /*!< Has WebSocket config */
    bool has_server_time;           /*!< Has server time */
} esp_iot_chat_http_info_t;

/**
* @brief  Handle for a IOT chat session.
*/
typedef uint32_t esp_iot_chat_handle_t;

/**
 * @brief  Events for IOT chat
 */
#define ESP_IOT_CHAT_EVENT_CONNECTED                (1 << 0)
#define ESP_IOT_CHAT_EVENT_DISCONNECTED             (1 << 1)
#define ESP_IOT_CHAT_EVENT_AUDIO_CHANNEL_OPENED     (1 << 2)
#define ESP_IOT_CHAT_EVENT_AUDIO_CHANNEL_CLOSED     (1 << 3)
#define ESP_IOT_CHAT_EVENT_AUDIO_DATA_INCOMING      (1 << 4)
#define ESP_IOT_CHAT_EVENT_SERVER_HELLO             (1 << 5)
#define ESP_IOT_CHAT_EVENT_SERVER_GOODBYE           (1 << 6)

/**
 * @brief  Events that can occur during a IOT chat session
 */
typedef enum {
    ESP_IOT_CHAT_EVENT_CHAT_CREATE = 0,               /*!< Triggered when a new chat session is created */
    ESP_IOT_CHAT_EVENT_CHAT_UPDATE,                   /*!< Triggered when there is an update in the chat session */
    ESP_IOT_CHAT_EVENT_CHAT_COMPLETED,                /*!< Triggered when the chat session is completed */
    ESP_IOT_CHAT_EVENT_CHAT_SPEECH_STARTED,           /*!< Triggered when speech output starts */
    ESP_IOT_CHAT_EVENT_CHAT_SPEECH_STOPED,            /*!< Triggered when speech output stops */
    ESP_IOT_CHAT_EVENT_CHAT_ERROR,                    /*!< Triggered when an error occurs during the chat session */
    ESP_IOT_CHAT_EVENT_INPUT_AUDIO_BUFFER_COMPLETED,  /*!< Triggered when the input audio buffer processing is completed */
    ESP_IOT_CHAT_EVENT_CHAT_SUBTITLE_EVENT,           /*!< Triggered when enabled `enable_subtitle`  */
    ESP_IOT_CHAT_EVENT_CHAT_CUSTOMER_DATA,            /*!< Triggered when custom user data is received  */
    ESP_IOT_CHAT_EVENT_CHAT_STATE,                    /*!< Triggered when state is changed */
    ESP_IOT_CHAT_EVENT_CHAT_TEXT,                     /*!< Triggered when text is received */
    ESP_IOT_CHAT_EVENT_CHAT_EMOJI,                    /*!< Triggered when emoji is received */
} esp_iot_chat_event_t;

/**
 * @brief  Supported audio formats for IOT chat
 */
typedef enum {
    ESP_IOT_CHAT_AUDIO_TYPE_PCM = 0,  /*!< Raw PCM audio format */
    ESP_IOT_CHAT_AUDIO_TYPE_OPUS,     /*!< OPUS compressed audio format */
    ESP_IOT_CHAT_AUDIO_TYPE_G711,     /*!< G.711 compressed audio format */
} esp_iot_chat_audio_type_t;

/**
 * @brief  Device state for IOT chat
 */
typedef enum {
    ESP_IOT_CHAT_DEVICE_STATE_UNKNOWN,
    ESP_IOT_CHAT_DEVICE_STATE_STARTING,
    ESP_IOT_CHAT_DEVICE_STATE_WIFI_CONFIGURING,
    ESP_IOT_CHAT_DEVICE_STATE_IDLE,
    ESP_IOT_CHAT_DEVICE_STATE_CONNECTING,
    ESP_IOT_CHAT_DEVICE_STATE_LISTENING,
    ESP_IOT_CHAT_DEVICE_STATE_SPEAKING,
    ESP_IOT_CHAT_DEVICE_STATE_UPGRADING,
    ESP_IOT_CHAT_DEVICE_STATE_ACTIVATING,
    ESP_IOT_CHAT_DEVICE_STATE_AUDIO_TESTING,
    ESP_IOT_CHAT_DEVICE_STATE_FATAL_ERROR
} esp_iot_chat_device_state_t;

/**
 * @brief  Listening mode for IOT chat
 */
typedef enum {
    ESP_IOT_CHAT_LISTENING_MODE_REALTIME,
    ESP_IOT_CHAT_LISTENING_MODE_AUTO,
    ESP_IOT_CHAT_LISTENING_MODE_MANUAL,
    ESP_IOT_CHAT_LISTENING_MODE_UNKNOWN,
} esp_iot_chat_listening_mode_t;

/**
 * @brief  Reasons for aborting speaking
 *
 */
typedef enum {
    ESP_IOT_CHAT_ABORT_SPEAKING_REASON_WAKE_WORD_DETECTED,
    ESP_IOT_CHAT_ABORT_SPEAKING_REASON_STOP_LISTENING,
    ESP_IOT_CHAT_ABORT_SPEAKING_REASON_UNKNOWN,
} esp_iot_chat_abort_speaking_reason_t;

typedef enum {
    ESP_IOT_CHAT_TEXT_ROLE_USER,
    ESP_IOT_CHAT_TEXT_ROLE_ASSISTANT,
} esp_iot_chat_text_role_t;

typedef struct {
    esp_iot_chat_text_role_t role;
    const char *text;
} esp_iot_chat_text_data_t;

/**
 * @brief  Callback for receiving audio data during chat
 *
 * @param data  Pointer to the audio data buffer
 * @param len   Length of the audio data in bytes
 * @param ctx   User-defined context passed to the callback
 */
typedef void (*esp_iot_chat_audio_callback_t)(uint8_t *data, int len, void *ctx);

/**
 * @brief  Callback for receiving chat events
 *
 * @param event       Chat event type
 * @param event_data  Optional output data associated with the event
 * @param ctx         User-defined context passed to the callback
 */
typedef void (*esp_iot_chat_event_callback_t)(esp_iot_chat_event_t event, void *event_data, void *ctx);

/**
 * @brief  Callback for receiving device state changed
 *
 * @param state   Device state
 * @param ctx     User-defined context passed to the callback
 */
typedef void (*esp_iot_chat_state_callback_t)(esp_iot_chat_device_state_t state, void *ctx);

#define ESP_IOT_CHAT_DEFAULT_CONFIG() {                              \
    .audio_input_task_core            = -1,                                \
    .audio_input_task_priority        = 5,                                  \
    .audio_input_task_stack_size      = 6 * 1024,                                \
    .audio_input_task_stack_in_ext = true, \
    .audio_callback            = NULL,                                \
    .event_callback            = NULL,                                \
    .audio_callback_ctx        = NULL,                                \
    .event_callback_ctx        = NULL,                                \
};

/**
 * @brief  Configuration structure for initializing a IOT chat session
 */
typedef struct {
    int audio_input_task_core;
    uint8_t audio_input_task_priority;
    uint32_t audio_input_task_stack_size;
    bool audio_input_task_stack_in_ext;
    esp_iot_chat_audio_callback_t   audio_callback;            /*!< Callback function for handling audio data */
    esp_iot_chat_event_callback_t   event_callback;            /*!< Callback function for handling IOT events */
    void                           *audio_callback_ctx;        /*!< Context pointer passed to the audio callback */
    void                           *event_callback_ctx;        /*!< Context pointer passed to the event callback */
} esp_iot_chat_config_t;

ESP_EVENT_DECLARE_BASE(ESP_IOT_CHAT_EVENTS);

/**
 * @brief  Instance the chat module
 *
 * @param[in]   config   Pointer to the chat configuration structure
 * @param[out]  chat_hd  Pointer to the chat handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_NO_MEM       Out of memory
 *       - ESP_ERR_INVALID_ARG  Invalid arguments
 */
esp_err_t esp_iot_chat_init(esp_iot_chat_config_t *config, esp_iot_chat_handle_t *chat_hd);

/**
 * @brief  Deinitialize the chat module
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
* @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_iot_chat_deinit(esp_iot_chat_handle_t chat_hd);

esp_err_t esp_iot_chat_get_http_info(esp_iot_chat_handle_t chat_hd, esp_iot_chat_http_info_t *info);

void esp_iot_chat_free_http_info(esp_iot_chat_http_info_t *info);

/**
 * @brief  Start the chat session
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_iot_chat_start(esp_iot_chat_handle_t chat_hd);

/**
 * @brief  Stop the chat session
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_iot_chat_stop(esp_iot_chat_handle_t chat_hd);

/**
 * @brief  Open audio channel
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 */
esp_err_t esp_iot_chat_open_audio_channel(esp_iot_chat_handle_t chat_hd, char *message, size_t message_len);

/**
 * @brief  Close audio channel
 *
 * @param[in]  chat_hd  Handle to the chat instance
 */
esp_err_t esp_iot_chat_close_audio_channel(esp_iot_chat_handle_t chat_hd);

/**
 * @brief  Send audio data to the chat session
 *
 * @param[in]  chat_hd  Handle to the chat instance
 * @param[in]  data     Pointer to the audio data buffer
 * @param[in]  data_len Length of the audio data in bytes
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_iot_chat_send_audio_data(esp_iot_chat_handle_t chat_hd, const char *data, size_t data_len);

/**
 * @brief  Send wake word detected
 *
 * @param[in]  chat_hd  Handle to the chat instance
 * @param[in]  wake_word  Pointer to the wake word
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_iot_chat_send_wake_word_detected(esp_iot_chat_handle_t chat_hd, const char *wake_word);

/**
 * @brief  Send start listening
 *
 * @param[in]  chat_hd  Handle to the chat instance
 * @param[in]  mode  Listening mode
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_iot_chat_send_start_listening(esp_iot_chat_handle_t chat_hd, esp_iot_chat_listening_mode_t mode);

/**
 * @brief  Send stop listening
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_iot_chat_send_stop_listening(esp_iot_chat_handle_t chat_hd);

/**
 * @brief  Send abort speaking
 *
 * @param[in]  chat_hd  Handle to the chat instance
 * @param[in]  reason  Reason for aborting speaking
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_iot_chat_send_abort_speaking(esp_iot_chat_handle_t chat_hd, esp_iot_chat_abort_speaking_reason_t reason);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
