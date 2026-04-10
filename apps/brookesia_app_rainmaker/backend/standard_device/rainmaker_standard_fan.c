/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rainmaker_standard_device.h"
#include "rainmaker_device_group_manager.h"
#include "esp_log.h"
#include "rainmaker_app_backend_util.h"
#include "rainmaker_api_handle.h"
static const char *TAG = "rainmaker_standard_fan";

esp_err_t rainmaker_standard_fan_set_power(const char *node_id, bool power_on, const char **out_err_reason)
{
    if (node_id == NULL) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"Fan\":{\"Power\":%s}}", power_on ? "true" : "false");
    return rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
}

esp_err_t rainmaker_standard_fan_set_speed(const char *node_id, int speed, const char **out_err_reason)
{
    if (node_id == NULL) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"Fan\":{\"Speed\":%d}}", speed);
    return rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
}

esp_err_t rainmaker_standard_fan_set_name(const char *node_id, const char *name, const char **out_err_reason)
{
    if (node_id == NULL || name == NULL || name[0] == '\0') {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID or empty name");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"Fan\":{\"Name\":\"%s\"}}", name);
    return rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
}

esp_err_t rainmaker_standard_fan_parse_parameters(cJSON *device, rm_device_item_t *device_item)
{
    if (device == NULL || device_item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *power = cJSON_GetObjectItem(device, "Power");
    if (!power || !cJSON_IsBool(power)) {
        ESP_LOGE(TAG, "Power is not a boolean");
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *speed = cJSON_GetObjectItem(device, "Speed");
    if (!speed || !cJSON_IsNumber(speed)) {
        ESP_LOGE(TAG, "Speed is not a number");
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *name = cJSON_GetObjectItem(device, "Name");
    if (!name || !cJSON_IsString(name) || !name->valuestring) {
        ESP_LOGE(TAG, "Name is not a string");
        return ESP_ERR_INVALID_ARG;
    }

    device_item->name = strdup(name->valuestring);
    if (!device_item->name) {
        ESP_LOGE(TAG, "Failed to allocate memory for name");
        return ESP_ERR_NO_MEM;
    }
    device_item->whether_has_power = true;
    device_item->power_on = (cJSON_IsTrue(power) ? true : false);
    device_item->type = RAINMAKER_APP_DEVICE_TYPE_FAN;
    device_item->fan.speed = speed->valueint;
    return ESP_OK;
}
