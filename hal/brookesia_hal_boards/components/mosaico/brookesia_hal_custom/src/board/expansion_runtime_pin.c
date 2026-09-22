/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdlib.h>

#include "esp_log.h"
#include "brookesia/hal_custom/macro_configs.h"
#include "brookesia/hal_custom/board/expansion.h"

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
#include "brookesia/hal_custom/expansion/mosaico_module_manager.h"
#endif

static const char *TAG = "MOSAICO_EXP_PIN";

typedef struct {
    bool initialized;
} expansion_runtime_pin_handle_t;

esp_err_t esp_mosaico_expansion_runtime_pin_init(void **device_handle)
{
    if (device_handle == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }
    *device_handle = NULL;

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
    expansion_runtime_pin_handle_t *handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (esp_mosaico_expansion_manager_cleanup_pending()) {
        /* The manager dependency already owns the live context. Recover it
         * without cycling BM references inside a device-init callback. */
        esp_err_t ret = esp_mosaico_expansion_manager_recover();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Recover expansion manager failed: %s", esp_err_to_name(ret));
            free(handle);
            return ret;
        }
        if (esp_mosaico_expansion_manager_cleanup_pending()) {
            ESP_LOGE(TAG, "Expansion manager still needs cleanup after recovery");
            free(handle);
            return ESP_ERR_INVALID_STATE;
        }
    }

    handle->initialized = true;
    *device_handle = handle;
    return ESP_OK;
#else
    ESP_LOGE(TAG, "Expansion module support is disabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_mosaico_expansion_runtime_pin_deinit(void *device_handle)
{
    if (device_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
    expansion_runtime_pin_handle_t *handle = device_handle;
    handle->initialized = false;
    free(handle);
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
