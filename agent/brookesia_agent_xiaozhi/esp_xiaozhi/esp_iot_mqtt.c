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
#include <esp_crt_bundle.h>

#include "esp_iot_settings.h"
#include "esp_iot_mqtt.h"

typedef struct {
    char publish_topic[ESP_IOT_MQTT_MAX_STRING_SIZE];
    char subscribe_topic[ESP_IOT_MQTT_MAX_STRING_SIZE];
    bool mqtt_connected;

    esp_mqtt_client_handle_t mqtt_client;
    esp_iot_mqtt_callbacks_t callbacks;
} esp_iot_mqtt_t;

static const char *TAG = "esp_iot_mqtt";

static void esp_iot_mqtt_event(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_iot_mqtt_t *mqtt = (esp_iot_mqtt_t *)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqtt->mqtt_connected = true;

        if (strlen(mqtt->subscribe_topic) > 0) {
            int msg_id = esp_mqtt_client_subscribe(mqtt->mqtt_client, mqtt->subscribe_topic, 0);
            ESP_LOGI(TAG, "Subscribed to topic %s, msg_id=%d", mqtt->subscribe_topic, msg_id);
        }

        if (mqtt->callbacks.on_connected_callback) {
            mqtt->callbacks.on_connected_callback((esp_iot_mqtt_handle_t)mqtt);
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt->mqtt_connected = false;

        if (mqtt->callbacks.on_disconnected_callback) {
            mqtt->callbacks.on_disconnected_callback((esp_iot_mqtt_handle_t)mqtt);
        }
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA, topic: %.*s, data: %.*s", (int)event->topic_len, event->topic, (int)event->data_len, event->data);
        if (mqtt->callbacks.on_message_callback) {
            mqtt->callbacks.on_message_callback((esp_iot_mqtt_handle_t)mqtt, event->topic, event->topic_len, event->data, event->data_len);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        break;

    default:
        ESP_LOGI(TAG, "Other MQTT event id: %d", event->event_id);
        break;
    }
}

esp_err_t esp_iot_mqtt_init(esp_iot_mqtt_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_iot_mqtt_t *mqtt = calloc(1, sizeof(esp_iot_mqtt_t));
    ESP_RETURN_ON_FALSE(mqtt, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for MQTT");

    *handle = (esp_iot_mqtt_handle_t)mqtt;

    return ESP_OK;
}

esp_err_t esp_iot_mqtt_deinit(esp_iot_mqtt_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_iot_mqtt_t *mqtt = (esp_iot_mqtt_t *)handle;
    if (mqtt->mqtt_client) {
        esp_mqtt_client_destroy(mqtt->mqtt_client);
        mqtt->mqtt_client = NULL;
    }

    free(mqtt);

    return ESP_OK;
}

esp_err_t esp_iot_mqtt_start(esp_iot_mqtt_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    char endpoint[ESP_IOT_MQTT_MAX_STRING_SIZE];
    char client_id[ESP_IOT_MQTT_MAX_STRING_SIZE];
    char username[ESP_IOT_MQTT_MAX_STRING_SIZE];
    char password[ESP_IOT_MQTT_MAX_STRING_SIZE];
    settings_handle_t settings;
    esp_err_t ret = ESP_OK;
    esp_iot_mqtt_t *mqtt = (esp_iot_mqtt_t *)handle;
    ret = settings_init(&settings, "mqtt", false);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Failed to initialize settings %s", esp_err_to_name(ret));

    settings_get_string(&settings, "endpoint", "", endpoint, sizeof(endpoint));
    settings_get_string(&settings, "client_id", "", client_id, sizeof(client_id));
    settings_get_string(&settings, "username", "", username, sizeof(username));
    settings_get_string(&settings, "password", "", password, sizeof(password));
    settings_get_string(&settings, "publish_topic", "", mqtt->publish_topic, sizeof(mqtt->publish_topic) - 1);
    settings_get_string(&settings, "subscribe_topic", "", mqtt->subscribe_topic, sizeof(mqtt->subscribe_topic) - 1);
    settings_deinit(&settings);
    ESP_RETURN_ON_FALSE(strlen(endpoint) > 0, ESP_ERR_INVALID_ARG, TAG, "MQTT endpoint is not specified");

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .hostname = endpoint,
                .port = 8883,
                .transport = MQTT_TRANSPORT_OVER_SSL,
            },
            .verification = {
                .crt_bundle_attach = esp_crt_bundle_attach,
            },
        },
        .credentials = {
            .client_id = client_id,
            .username = username,
            .authentication = {
                .password = password,
            },
        },
        .session = {
            .keepalive = 90,
        },
    };

    if (mqtt->mqtt_client) {
        ESP_LOGW(TAG, "MQTT client already started");
        esp_mqtt_client_destroy(mqtt->mqtt_client);
        mqtt->mqtt_client = NULL;
    }

    mqtt->mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_RETURN_ON_FALSE(mqtt->mqtt_client, ESP_ERR_INVALID_ARG, TAG, "Failed to initialize MQTT client");

    esp_mqtt_client_register_event(mqtt->mqtt_client, MQTT_EVENT_ANY, esp_iot_mqtt_event, mqtt);
    ret = esp_mqtt_client_start(mqtt->mqtt_client);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));

    ESP_LOGI(TAG, "MQTT client started successfully");
    return ESP_OK;
}

esp_err_t esp_iot_mqtt_stop(esp_iot_mqtt_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_iot_mqtt_t *mqtt = (esp_iot_mqtt_t *)handle;
    esp_mqtt_client_stop(mqtt->mqtt_client);
    return ESP_OK;
}

esp_err_t esp_iot_mqtt_send_text(esp_iot_mqtt_handle_t handle, const char *text)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(text, ESP_ERR_INVALID_ARG, TAG, "Invalid text");

    esp_iot_mqtt_t *mqtt = (esp_iot_mqtt_t *)handle;
    ESP_RETURN_ON_FALSE(strlen(mqtt->publish_topic) > 0, ESP_ERR_INVALID_ARG, TAG, "Publish topic is not specified");

    ESP_LOGI(TAG, "esp_iot_mqtt_send_text data: %s", text);
    int msg_id = esp_mqtt_client_publish(mqtt->mqtt_client, mqtt->publish_topic, text, 0, 0, 0);
    ESP_RETURN_ON_FALSE(msg_id != -1, ESP_ERR_INVALID_ARG, TAG, "Failed to publish message: %s", text);

    return ESP_OK;
}

esp_err_t esp_iot_mqtt_set_callbacks(esp_iot_mqtt_handle_t handle, esp_iot_mqtt_callbacks_t callbacks)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_iot_mqtt_t *mqtt = (esp_iot_mqtt_t *)handle;
    mqtt->callbacks = callbacks;

    return ESP_OK;
}
