/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_iot_settings.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "esp_iot_settings";

esp_err_t settings_init(settings_handle_t *handle, const char *namespace_name, bool read_write)
{
    if (handle == NULL || namespace_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize handle
    memset(handle, 0, sizeof(settings_handle_t));

    // Copy namespace name
    size_t ns_len = strlen(namespace_name) + 1;
    handle->namespace_name = (char *)malloc(ns_len);
    if (handle->namespace_name == NULL) {
        return ESP_ERR_NO_MEM;
    }
    strcpy(handle->namespace_name, namespace_name);

    handle->read_write = read_write;
    handle->dirty = false;

    // Open NVS handle
    esp_err_t ret = nvs_open(namespace_name, read_write ? NVS_READWRITE : NVS_READONLY, &handle->nvs_handle);
    if (ret != ESP_OK) {
        free(handle->namespace_name);
        handle->namespace_name = NULL;
        return ret;
    }

    return ESP_OK;
}

void settings_deinit(settings_handle_t *handle)
{
    if (handle == NULL) {
        return;
    }

    if (handle->nvs_handle != 0) {
        if (handle->read_write && handle->dirty) {
            esp_err_t ret = nvs_commit(handle->nvs_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
            }
        }
        nvs_close(handle->nvs_handle);
        handle->nvs_handle = 0;
    }

    if (handle->namespace_name != NULL) {
        free(handle->namespace_name);
        handle->namespace_name = NULL;
    }

    handle->read_write = false;
    handle->dirty = false;
}

esp_err_t settings_get_string(settings_handle_t *handle, const char *key,
                              const char *default_value, char *out_value, size_t max_len)
{
    if (handle == NULL || key == NULL || out_value == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->nvs_handle == 0) {
        if (default_value != NULL) {
            strncpy(out_value, default_value, max_len - 1);
            out_value[max_len - 1] = '\0';
        } else {
            out_value[0] = '\0';
        }
        return ESP_OK;
    }

    size_t length = max_len;
    esp_err_t ret = nvs_get_str(handle->nvs_handle, key, out_value, &length);
    if (ret != ESP_OK) {
        if (default_value != NULL) {
            strncpy(out_value, default_value, max_len - 1);
            out_value[max_len - 1] = '\0';
        } else {
            out_value[0] = '\0';
        }
        return ESP_OK;  // Return success even if key not found, using default value
    }

    return ESP_OK;
}

esp_err_t settings_set_string(settings_handle_t *handle, const char *key, const char *value)
{
    if (handle == NULL || key == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->read_write) {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", handle->namespace_name);
        return ESP_ERR_NOT_ALLOWED;
    }

    if (handle->nvs_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_set_str(handle->nvs_handle, key, value);
    if (ret == ESP_OK) {
        handle->dirty = true;
    }

    return ret;
}

esp_err_t settings_get_int(settings_handle_t *handle, const char *key,
                           int32_t default_value, int32_t *out_value)
{
    if (handle == NULL || key == NULL || out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->nvs_handle == 0) {
        *out_value = default_value;
        return ESP_OK;
    }

    esp_err_t ret = nvs_get_i32(handle->nvs_handle, key, out_value);
    if (ret != ESP_OK) {
        *out_value = default_value;
        return ESP_OK;  // Return success even if key not found, using default value
    }

    return ESP_OK;
}

esp_err_t settings_set_int(settings_handle_t *handle, const char *key, int32_t value)
{
    if (handle == NULL || key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->read_write) {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", handle->namespace_name);
        return ESP_ERR_NOT_ALLOWED;
    }

    if (handle->nvs_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_set_i32(handle->nvs_handle, key, value);
    if (ret == ESP_OK) {
        handle->dirty = true;
    }

    return ret;
}

esp_err_t settings_erase_key(settings_handle_t *handle, const char *key)
{
    if (handle == NULL || key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->read_write) {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", handle->namespace_name);
        return ESP_ERR_NOT_ALLOWED;
    }

    if (handle->nvs_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_erase_key(handle->nvs_handle, key);
    if (ret != ESP_ERR_NVS_NOT_FOUND) {
        if (ret == ESP_OK) {
            handle->dirty = true;
        }
        return ret;
    }

    return ESP_OK;  // Key not found is not an error
}

esp_err_t settings_erase_all(settings_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->read_write) {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", handle->namespace_name);
        return ESP_ERR_NOT_ALLOWED;
    }

    if (handle->nvs_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_erase_all(handle->nvs_handle);
    if (ret == ESP_OK) {
        handle->dirty = true;
    }

    return ret;
}

esp_err_t settings_commit(settings_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->read_write) {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", handle->namespace_name);
        return ESP_ERR_NOT_ALLOWED;
    }

    if (handle->nvs_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_commit(handle->nvs_handle);
    if (ret == ESP_OK) {
        handle->dirty = false;
    }

    return ret;
}
