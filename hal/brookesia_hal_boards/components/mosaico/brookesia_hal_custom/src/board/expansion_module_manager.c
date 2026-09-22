/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>

#include "esp_log.h"
#include "brookesia/hal_custom/macro_configs.h"
#include "brookesia/hal_custom/board/expansion.h"

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
#include "brookesia/hal_custom/expansion/mosaico_module_manager.h"
#endif

static const char *TAG = "MOSAICO_EXPANSION";

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
typedef struct {
    bool initialized;
} expansion_module_manager_handle_t;

/* BM custom teardown discards its wrapper even on error. The manager and this
 * wrapper therefore live for the board lifetime and retain pending ownership. */
static expansion_module_manager_handle_t manager_handle;
#endif
static esp_err_t cleanup_error;

esp_err_t esp_mosaico_expansion_cleanup_error(void)
{
    return cleanup_error;
}

esp_err_t esp_mosaico_expansion_cleanup(void)
{
#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
    cleanup_error = esp_mosaico_expansion_manager_deinit();
    if (cleanup_error == ESP_OK) {
        manager_handle.initialized = false;
    }
    return cleanup_error;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_mosaico_expansion_module_manager_init(
    const esp_mosaico_expansion_module_manager_config_t *config, void **device_handle
)
{
    if ((config == NULL) || (device_handle == NULL)) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
    if ((config->peripheral_count != 1) || (config->peripheral_name == NULL) ||
            (config->frequency_hz <= 0) || (config->timeout_ms <= 0)) {
        ESP_LOGE(TAG, "Invalid expansion manager configuration");
        return ESP_ERR_INVALID_ARG;
    }

    expansion_module_manager_handle_t *handle = &manager_handle;
    if (cleanup_error != ESP_OK) {
        esp_err_t ret = esp_mosaico_expansion_cleanup();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    const esp_mosaico_expansion_manager_config_t mosaico_config = {
        .i2c_name = config->peripheral_name,
        .frequency_hz = config->frequency_hz,
        .timeout_ms = config->timeout_ms,
        .slots = {
            [ESP_MOSAICO_EXPANSION_SLOT_LEFT] = {
                .address_gpio_num = config->left_address_gpio_num,
                .address_level = config->left_address_level,
                .eeprom_address = config->left_eeprom_address,
            },
            [ESP_MOSAICO_EXPANSION_SLOT_RIGHT] = {
                .address_gpio_num = config->right_address_gpio_num,
                .address_level = config->right_address_level,
                .eeprom_address = config->right_eeprom_address,
            },
        },
    };
    esp_err_t ret = esp_mosaico_expansion_manager_init(&mosaico_config);
    if (ret != ESP_OK) {
        return ret;
    }

    handle->initialized = true;
    *device_handle = handle;
    return ESP_OK;
#else
    ESP_LOGE(TAG, "Expansion module support is disabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_mosaico_expansion_module_manager_deinit(void *device_handle)
{
    if (device_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
    return esp_mosaico_expansion_cleanup();
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
