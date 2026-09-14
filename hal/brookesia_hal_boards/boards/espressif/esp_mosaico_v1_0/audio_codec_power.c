/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gen_board_device_custom.h"

static const char *TAG = "MOSAICO_CODEC_POWER";

typedef struct {
    gpio_num_t gpio_num;
} audio_codec_power_handle_t;

static esp_err_t configure_power_output(gpio_num_t gpio_num, uint32_t level)
{
    const gpio_config_t output_config = {
        .pin_bit_mask = 1ULL << gpio_num,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_reset_pin(gpio_num);
    if (ret != ESP_OK) {
        return ret;
    }

    // Program the output latch before enabling the pin to avoid a power glitch.
    ret = gpio_set_level(gpio_num, level);
    if (ret != ESP_OK) {
        return ret;
    }
    return gpio_config(&output_config);
}

static int audio_codec_power_init(void *config, int cfg_size, void **device_handle)
{
    if (config == NULL || device_handle == NULL ||
            cfg_size != (int)sizeof(dev_custom_audio_codec_power_config_t)) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    const dev_custom_audio_codec_power_config_t *power_config = config;
    audio_codec_power_handle_t *handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    handle->gpio_num = power_config->codec_gpio_num;

    esp_err_t ret = configure_power_output(handle->gpio_num, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable codec power: %s", esp_err_to_name(ret));
        (void)configure_power_output(handle->gpio_num, 0);
        free(handle);
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(power_config->settle_time_ms));
    *device_handle = handle;
    return ESP_OK;
}

static int audio_codec_power_deinit(void *device_handle)
{
    if (device_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    audio_codec_power_handle_t *handle = device_handle;
    esp_err_t ret = configure_power_output(handle->gpio_num, 0);
    free(handle);
    return ret;
}

CUSTOM_DEVICE_IMPLEMENT(audio_codec_power, audio_codec_power_init, audio_codec_power_deinit);
