/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#include <stdlib.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "gen_board_device_custom.h"

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
#include "brookesia/hal_custom/expansion/mosaico_module_manager.h"
#endif

static const char *TAG = "MOSAICO_EXPANSION";

typedef struct {
    bool initialized;
} expansion_module_manager_handle_t;

static int expansion_module_manager_init(void *config, int cfg_size, void **device_handle)
{
    if ((config == NULL) || (device_handle == NULL) ||
            (cfg_size != (int)sizeof(dev_custom_expansion_module_manager_config_t))) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
    const dev_custom_expansion_module_manager_config_t *manager_config = config;
    if ((manager_config->peripheral_count != 1) || (manager_config->peripheral_name == NULL) ||
            (manager_config->frequency_hz <= 0) || (manager_config->timeout_ms <= 0)) {
        ESP_LOGE(TAG, "Invalid expansion manager configuration");
        return ESP_ERR_INVALID_ARG;
    }

    expansion_module_manager_handle_t *handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const esp_mosaico_expansion_manager_config_t mosaico_config = {
        .i2c_name = manager_config->peripheral_name,
        .frequency_hz = manager_config->frequency_hz,
        .timeout_ms = manager_config->timeout_ms,
        .slots = {
            [ESP_MOSAICO_EXPANSION_SLOT_LEFT] = {
                .address_gpio_num = manager_config->left_address_gpio_num,
                .address_level = manager_config->left_address_level,
                .eeprom_address = manager_config->left_eeprom_address,
            },
            [ESP_MOSAICO_EXPANSION_SLOT_RIGHT] = {
                .address_gpio_num = manager_config->right_address_gpio_num,
                .address_level = manager_config->right_address_level,
                .eeprom_address = manager_config->right_eeprom_address,
            },
        },
    };
    esp_err_t ret = esp_mosaico_expansion_manager_init(&mosaico_config);
    if (ret != ESP_OK) {
        free(handle);
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

static int expansion_module_manager_deinit(void *device_handle)
{
    if (device_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
    expansion_module_manager_handle_t *handle = device_handle;
    esp_err_t ret = esp_mosaico_expansion_manager_deinit();
    if (ret != ESP_OK) {
        return ret;
    }
    handle->initialized = false;
    free(handle);
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

CUSTOM_DEVICE_IMPLEMENT(
    expansion_module_manager, expansion_module_manager_init, expansion_module_manager_deinit
);
