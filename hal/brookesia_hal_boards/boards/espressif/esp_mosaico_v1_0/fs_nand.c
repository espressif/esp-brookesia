/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
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

    esp_err_t cleanup_ret;
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
    cleanup_ret = spi_bus_remove_device(handle->spi_device);
    if (cleanup_ret != ESP_OK) {
        /* Keep the SPI bus referenced while it still owns the device. */
        ESP_LOGE(TAG, "Failed to remove NAND SPI device during rollback: %s", esp_err_to_name(cleanup_ret));
        goto fail_reset_pins;
    }
    handle->spi_device = NULL;
fail_unref:
    cleanup_ret = esp_board_periph_unref_handle(handle->peripheral_name);
    if (cleanup_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to release NAND SPI peripheral during rollback: %s", esp_err_to_name(cleanup_ret));
    }
fail_reset_pins:
    cleanup_ret = gpio_reset_pin(handle->hold_gpio_num);
    if (cleanup_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset NAND HOLD GPIO during rollback: %s", esp_err_to_name(cleanup_ret));
    }
    cleanup_ret = gpio_reset_pin(handle->wp_gpio_num);
    if (cleanup_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset NAND WP GPIO during rollback: %s", esp_err_to_name(cleanup_ret));
    }
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

    /*
     * Board Manager consumes custom device handles even when their deinit
     * callback reports an error. BDL release also consumes its handle regardless
     * of its return value. Cleanup is therefore best-effort and must not report
     * failure after this wrapper is freed.
     */
    if (handle->bdl_handle != NULL) {
        esp_blockdev_handle_t bdl_handle = handle->bdl_handle;
        esp_err_t ret = bdl_handle->ops->sync(bdl_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to sync NAND BDL during deinit: %s", esp_err_to_name(ret));
        }
        handle->bdl_handle = NULL;
        ret = bdl_handle->ops->release(bdl_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to release NAND BDL: %s", esp_err_to_name(ret));
        }
    }

    bool can_release_peripheral = true;
    if (handle->spi_device != NULL) {
        esp_err_t ret = spi_bus_remove_device(handle->spi_device);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to remove NAND SPI device: %s", esp_err_to_name(ret));
            can_release_peripheral = false;
        } else {
            handle->spi_device = NULL;
        }
    }

    esp_err_t ret = ESP_OK;
    if (can_release_peripheral) {
        ret = esp_board_periph_unref_handle(handle->peripheral_name);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to release NAND SPI peripheral: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "Keeping NAND SPI peripheral referenced because its device is still active");
    }

    ret = gpio_reset_pin(handle->hold_gpio_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset NAND HOLD GPIO: %s", esp_err_to_name(ret));
    }
    ret = gpio_reset_pin(handle->wp_gpio_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset NAND WP GPIO: %s", esp_err_to_name(ret));
    }
    free(handle);
    return ESP_OK;
}

CUSTOM_DEVICE_IMPLEMENT(fs_nand, fs_nand_init, fs_nand_deinit);
