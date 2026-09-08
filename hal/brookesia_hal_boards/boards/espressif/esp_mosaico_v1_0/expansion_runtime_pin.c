/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#include <stdbool.h>
#include <stdlib.h>

#include "esp_board_manager.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "gen_board_device_custom.h"

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
#include "brookesia/hal_custom/expansion/mosaico_module_manager.h"
#endif

static const char *TAG = "MOSAICO_EXP_PIN";
static const char *MANAGER_DEVICE_NAME = "expansion_module_manager";

typedef struct {
    bool initialized;
} expansion_runtime_pin_handle_t;

static int expansion_runtime_pin_init(void *config, int cfg_size, void **device_handle)
{
    if ((config == NULL) || (device_handle == NULL) ||
            (cfg_size != (int)sizeof(dev_custom_expansion_runtime_pin_config_t))) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
    expansion_runtime_pin_handle_t *handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (esp_mosaico_expansion_manager_cleanup_pending()) {
        /* Board Manager 0.5.15's dev_custom_deinit() logs but hides a board
         * custom-deinit error. It can therefore clear the BM manager handle and
         * reference while the low-level context remains cleanup-pending. The
         * manager dependency was just acquired for this pin; cycle that
         * reference once to retry low-level cleanup before recovery. This
         * callback runs before the pin gets a device handle, so the dependent
         * check cannot reject the balancing release because of the pin itself. */
        esp_err_t ret = esp_board_manager_deinit_device_by_name(MANAGER_DEVICE_NAME);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Release stale expansion manager reference failed: %s",
                     esp_err_to_name(ret));
            free(handle);
            return ret;
        }

        /* dev_custom_deinit() in Board Manager 0.5.15 deliberately hides a
         * board custom-deinit error and clears the BM handle/reference anyway.
         * Always reacquire this dependency after the balancing release; the
         * low-level initialized flag is therefore not a valid BM-ref test. */
        ret = esp_board_manager_init_device_by_name(MANAGER_DEVICE_NAME);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Restore expansion manager dependency failed: %s",
                     esp_err_to_name(ret));
            free(handle);
            return ret;
        }

        ret = esp_mosaico_expansion_manager_recover();
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

static int expansion_runtime_pin_deinit(void *device_handle)
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

CUSTOM_DEVICE_IMPLEMENT(expansion_runtime_pin, expansion_runtime_pin_init, expansion_runtime_pin_deinit);
