/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "brookesia/hal_custom/board/power.h"

static const char *TAG = "MOSAICO_POWER";

typedef struct {
    gpio_num_t vcc_gpio_num;
    gpio_num_t shutdown_gpio_num;
} board_power_handle_t;

static esp_err_t configure_output(gpio_num_t gpio_num, gpio_mode_t mode, uint32_t level);
static esp_err_t ramp_vcc_on(gpio_num_t gpio_num, uint32_t frequency_hz, uint32_t ramp_time_ms);

esp_err_t esp_mosaico_power_init(const esp_mosaico_power_config_t *config, void **device_handle)
{
    if (config == NULL || device_handle == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    const esp_mosaico_power_config_t *power_config = config;
    board_power_handle_t *handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    handle->vcc_gpio_num = power_config->vcc_gpio_num;
    handle->shutdown_gpio_num = power_config->shutdown_gpio_num;

    esp_err_t ret = configure_output(handle->shutdown_gpio_num, GPIO_MODE_OUTPUT_OD, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to release shutdown control: %s", esp_err_to_name(ret));
        goto fail;
    }
    ret = configure_output(handle->vcc_gpio_num, GPIO_MODE_OUTPUT, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to keep VCC rail off: %s", esp_err_to_name(ret));
        goto fail;
    }
    ret = ramp_vcc_on(
              handle->vcc_gpio_num, power_config->ramp_frequency_hz, power_config->ramp_time_ms
          );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to ramp VCC rail: %s", esp_err_to_name(ret));
        goto fail;
    }

    *device_handle = handle;
    return ESP_OK;

fail:
    configure_output(handle->vcc_gpio_num, GPIO_MODE_OUTPUT, 1);
    configure_output(handle->shutdown_gpio_num, GPIO_MODE_OUTPUT_OD, 1);
    free(handle);
    return ret;
}

esp_err_t esp_mosaico_power_deinit(void *device_handle)
{
    if (device_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    board_power_handle_t *handle = device_handle;
    esp_err_t ret = configure_output(handle->vcc_gpio_num, GPIO_MODE_OUTPUT, 1);
    esp_err_t shutdown_ret = configure_output(handle->shutdown_gpio_num, GPIO_MODE_OUTPUT_OD, 1);
    free(handle);

    if (ret != ESP_OK) {
        return ret;
    }
    return shutdown_ret;
}

static esp_err_t configure_output(gpio_num_t gpio_num, gpio_mode_t mode, uint32_t level)
{
    const gpio_config_t output_config = {
        .pin_bit_mask = 1ULL << gpio_num,
                             .mode = mode,
                             .pull_up_en = GPIO_PULLUP_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_reset_pin(gpio_num);
    if (ret != ESP_OK) {
        return ret;
    }

    // Program the output latch before enabling the pin to avoid a level glitch.
    ret = gpio_set_level(gpio_num, level);
    if (ret != ESP_OK) {
        return ret;
    }
    return gpio_config(&output_config);
}

static esp_err_t ramp_vcc_on(gpio_num_t gpio_num, uint32_t frequency_hz, uint32_t ramp_time_ms)
{
    bool timer_configured = false;
    bool channel_configured = false;
    bool fade_installed_here = false;
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = frequency_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer_config);
    if (ret != ESP_OK) {
        return ret;
    }
    timer_configured = true;

    // GPIO60 is held high before the ramp. Release the GPIO reservation before
    // handing the same pin to LEDC so the ownership check does not self-conflict.
    ret = gpio_reset_pin(gpio_num);
    if (ret != ESP_OK) {
        goto cleanup;
    }

    const ledc_channel_config_t channel_config = {
        .gpio_num = gpio_num,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 255,
        .hpoint = 0,
    };
    ret = ledc_channel_config(&channel_config);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    channel_configured = true;

    ret = ledc_fade_func_install(0);
    if (ret == ESP_OK) {
        fade_installed_here = true;
    } else if (ret != ESP_ERR_INVALID_STATE) {
        goto cleanup;
    }

    ret = ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0, ramp_time_ms);
    if (ret == ESP_OK) {
        ret = ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_WAIT_DONE);
    }

cleanup:
    if (channel_configured) {
        const uint32_t idle_level = (ret == ESP_OK) ? 0 : 1;
        esp_err_t cleanup_ret = ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, idle_level);
        if ((ret == ESP_OK) && (cleanup_ret != ESP_OK)) {
            ret = cleanup_ret;
        }
    }
    if (fade_installed_here) {
        ledc_fade_func_uninstall();
    }

    if (channel_configured) {
        const ledc_channel_config_t channel_deconfig = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .deconfigure = true,
        };
        esp_err_t cleanup_ret = ledc_channel_config(&channel_deconfig);
        if ((ret == ESP_OK) && (cleanup_ret != ESP_OK)) {
            ret = cleanup_ret;
        }
    }

    if (timer_configured) {
        esp_err_t cleanup_ret = ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1);
        if ((ret == ESP_OK) && (cleanup_ret != ESP_OK)) {
            ret = cleanup_ret;
        }
        const ledc_timer_config_t timer_deconfig = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = LEDC_TIMER_1,
            .deconfigure = true,
        };
        cleanup_ret = ledc_timer_config(&timer_deconfig);
        if ((ret == ESP_OK) && (cleanup_ret != ESP_OK)) {
            ret = cleanup_ret;
        }
    }

    esp_err_t output_ret = configure_output(
                               gpio_num, GPIO_MODE_OUTPUT, (ret == ESP_OK) ? 0 : 1
                           );
    if ((ret == ESP_OK) && (output_ret != ESP_OK)) {
        ret = output_ret;
    }
    return ret;
}
