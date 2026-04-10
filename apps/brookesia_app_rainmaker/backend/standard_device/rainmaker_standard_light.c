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

static const char *TAG = "rainmaker_standard_light";

esp_err_t rainmaker_standard_light_set_power(const char *node_id, bool power_on, const char **out_err_reason)
{
    if (node_id == NULL) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"Light\":{\"Power\":%s}}", power_on ? "true" : "false");
    return rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
}

esp_err_t rainmaker_standard_light_set_brightness(const char *node_id, int brightness, const char **out_err_reason)
{
    if (node_id == NULL) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"Light\":{\"Brightness\":%d}}", brightness);
    return rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
}

esp_err_t rainmaker_standard_light_set_hue(const char *node_id, int hue, const char **out_err_reason)
{
    if (node_id == NULL) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"Light\":{\"Hue\":%d}}", hue);
    return rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
}

esp_err_t rainmaker_standard_light_set_saturation(const char *node_id, int saturation, const char **out_err_reason)
{
    if (node_id == NULL) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"Light\":{\"Saturation\":%d}}", saturation);
    return rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
}

esp_err_t rainmaker_standard_light_set_name(const char *node_id, const char *name, const char **out_err_reason)
{
    if (node_id == NULL || name == NULL || name[0] == '\0') {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID or empty name");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"Light\":{\"Name\":\"%s\"}}", name);
    return rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
}

esp_err_t rainmaker_standard_light_parse_parameters(cJSON *device, rm_device_item_t *device_item)
{
    if (device == NULL || device_item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *power = cJSON_GetObjectItem(device, "Power");
    if (!power || !cJSON_IsBool(power)) {
        ESP_LOGE(TAG, "Power is not a boolean");
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *brightness = cJSON_GetObjectItem(device, "Brightness");
    if (!brightness || !cJSON_IsNumber(brightness)) {
        ESP_LOGE(TAG, "Brightness is not a number");
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *hue = cJSON_GetObjectItem(device, "Hue");
    if (!hue || !cJSON_IsNumber(hue)) {
        ESP_LOGE(TAG, "Hue is not a number");
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *saturation = cJSON_GetObjectItem(device, "Saturation");
    if (!saturation || !cJSON_IsNumber(saturation)) {
        ESP_LOGE(TAG, "Saturation is not a number");
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
    device_item->type = RAINMAKER_APP_DEVICE_TYPE_LIGHT;
    device_item->light.brightness = brightness->valueint;
    device_item->light.hue = hue->valueint;
    device_item->light.saturation = saturation->valueint;
    return ESP_OK;
}
