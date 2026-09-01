/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_board_periph.h"
#include "esp_err.h"
#include "esp_log.h"
#include "fs_nand.h"
#include "gen_board_device_custom.h"
#include "periph_spi.h"
#include "spi_nand_flash.h"

static const char *TAG = "MOSAICO_NAND";

typedef struct {
    spi_device_handle_t spi_device;
    esp_blockdev_handle_t bdl_handle;
    const char *peripheral_name;
    gpio_num_t hold_gpio_num;
    gpio_num_t wp_gpio_num;
} fs_nand_handle_t;

static esp_err_t configure_protect_pins(gpio_num_t hold_gpio_num, gpio_num_t wp_gpio_num)
{
    const gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << hold_gpio_num) | (1ULL << wp_gpio_num),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&output_config);
    if (ret == ESP_OK) {
        ret = gpio_set_level(hold_gpio_num, 1);
    }
    if (ret == ESP_OK) {
        ret = gpio_set_level(wp_gpio_num, 1);
    }
    return ret;
}

esp_err_t fs_nand_get_bdl_handle(void *device_handle, esp_blockdev_handle_t *out_handle)
{
    if (device_handle == NULL || out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    fs_nand_handle_t *handle = device_handle;
    if (handle->bdl_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    *out_handle = handle->bdl_handle;
    return ESP_OK;
}

static int fs_nand_init(void *config, int cfg_size, void **device_handle)
{
#if !CONFIG_NAND_FLASH_ENABLE_BDL
    (void)config;
    (void)cfg_size;
    (void)device_handle;
    ESP_LOGE(TAG, "NAND BDL support is disabled");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (config == NULL || device_handle == NULL || cfg_size != (int)sizeof(dev_custom_fs_nand_config_t)) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    const dev_custom_fs_nand_config_t *nand_config = config;
    fs_nand_handle_t *handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    handle->peripheral_name = nand_config->peripheral_name;
    handle->hold_gpio_num = nand_config->hold_gpio_num;
    handle->wp_gpio_num = nand_config->wp_gpio_num;

    esp_err_t ret = configure_protect_pins(handle->hold_gpio_num, handle->wp_gpio_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to release NAND HOLD/WP: %s", esp_err_to_name(ret));
        goto fail_reset_pins;
    }

    periph_spi_handle_t *spi_bus = NULL;
    ret = esp_board_periph_ref_handle(handle->peripheral_name, (void **)&spi_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire NAND SPI bus: %s", esp_err_to_name(ret));
        goto fail_reset_pins;
    }

    const spi_device_interface_config_t spi_config = {
        .clock_speed_hz = nand_config->clock_speed_hz,
        .mode = 0,
        .spics_io_num = nand_config->cs_gpio_num,
        .queue_size = nand_config->queue_size,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    ret = spi_bus_add_device(spi_bus->spi_port, &spi_config, &handle->spi_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add NAND to SPI bus: %s", esp_err_to_name(ret));
        goto fail_unref;
    }

    spi_nand_flash_config_t flash_config = {
        .device_handle = handle->spi_device,
        .gc_factor = nand_config->gc_factor,
        .io_mode = SPI_NAND_IO_MODE_SIO,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    ret = spi_nand_flash_init_with_layers(&flash_config, &handle->bdl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NAND BDL layers: %s", esp_err_to_name(ret));
        goto fail_remove_device;
    }

    *device_handle = handle;
    return ESP_OK;

fail_remove_device:
    spi_bus_remove_device(handle->spi_device);
fail_unref:
    esp_board_periph_unref_handle(handle->peripheral_name);
fail_reset_pins:
    (void)gpio_reset_pin(handle->hold_gpio_num);
    (void)gpio_reset_pin(handle->wp_gpio_num);
    free(handle);
    return ret;
#endif
}

static int fs_nand_deinit(void *device_handle)
{
    if (device_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    fs_nand_handle_t *handle = device_handle;
    esp_err_t first_error = ESP_OK;
    if (handle->bdl_handle != NULL) {
        esp_err_t ret = handle->bdl_handle->ops->sync(handle->bdl_handle);
        if (ret != ESP_OK) {
            first_error = ret;
        }
        ret = handle->bdl_handle->ops->release(handle->bdl_handle);
        if (ret != ESP_OK) {
            first_error = first_error == ESP_OK ? ret : first_error;
        }
        handle->bdl_handle = NULL;
    }
    if (handle->spi_device != NULL) {
        esp_err_t ret = spi_bus_remove_device(handle->spi_device);
        if (first_error == ESP_OK) {
            first_error = ret;
        }
    }
    esp_err_t ret = esp_board_periph_unref_handle(handle->peripheral_name);
    if (first_error == ESP_OK) {
        first_error = ret;
    }
    ret = gpio_reset_pin(handle->hold_gpio_num);
    if (first_error == ESP_OK) {
        first_error = ret;
    }
    ret = gpio_reset_pin(handle->wp_gpio_num);
    if (first_error == ESP_OK) {
        first_error = ret;
    }
    free(handle);
    return first_error;
}

CUSTOM_DEVICE_IMPLEMENT(fs_nand, fs_nand_init, fs_nand_deinit);
