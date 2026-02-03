/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cJSON.h>
#include <mbedtls/aes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <mqtt_client.h>
#include <esp_crt_bundle.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */


#define ESP_IOT_MQTT_MAX_STRING_SIZE 256
#define ESP_IOT_MQTT_MAX_PAYLOAD_SIZE 2048
#define ESP_IOT_MQTT_AES_NONCE_SIZE 16
#define ESP_IOT_MQTT_AES_KEY_SIZE 32

typedef uint32_t esp_iot_mqtt_handle_t;

/**
 * @brief Callback functions structure for MQTT operations
 */
typedef struct {
    esp_err_t (*on_message_callback)(esp_iot_mqtt_handle_t handle, const char *topic, size_t topic_len, const char *data, size_t data_len);                  /*!< Open function */
    esp_err_t (*on_connected_callback)(esp_iot_mqtt_handle_t handle);
    esp_err_t (*on_disconnected_callback)(esp_iot_mqtt_handle_t handle);                                               /*!< Close function */
} esp_iot_mqtt_callbacks_t;

/**
 * @brief  Initialize the mqtt application
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t esp_iot_mqtt_init(esp_iot_mqtt_handle_t *handle);

/**
 * @brief  Deinitialize the mqtt application
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t esp_iot_mqtt_deinit(esp_iot_mqtt_handle_t handle);

/**
 * @brief  Start the mqtt application
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t esp_iot_mqtt_start(esp_iot_mqtt_handle_t handle);

/**
 * @brief  Stop the mqtt application
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t esp_iot_mqtt_stop(esp_iot_mqtt_handle_t handle);

/**
 * @brief  Send text to the mqtt application
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t esp_iot_mqtt_send_text(esp_iot_mqtt_handle_t handle, const char *text);

/**
 * @brief  Set the callbacks for the mqtt application
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t esp_iot_mqtt_set_callbacks(esp_iot_mqtt_handle_t handle, esp_iot_mqtt_callbacks_t callbacks);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
