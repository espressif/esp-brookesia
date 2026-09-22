/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "periph_spi.h"
#include "spi_nand_flash.h"
#include "brookesia/hal_adaptor/board_manager.h"
#include "brookesia/hal_custom/board/nand.h"

static const char *TAG = "MOSAICO_NAND";

typedef struct {
    spi_device_handle_t spi_device;
    esp_blockdev_handle_t bdl_handle;
    const char *peripheral_name;
    gpio_num_t hold_gpio_num;
    gpio_num_t wp_gpio_num;
    bool peripheral_referenced;
    bool hold_pin_owned;
    bool wp_pin_owned;
    bool cleanup_started;
    esp_err_t peripheral_error;
    esp_err_t consumed_error;
} fs_nand_handle_t;

static fs_nand_handle_t *pending_cleanup;
static esp_err_t last_cleanup_error;

static esp_err_t cleanup_handle(fs_nand_handle_t *handle);

static esp_err_t configure_protect_pins(gpio_num_t hold_gpio_num, gpio_num_t wp_gpio_num);

esp_err_t fs_nand_get_bdl_handle(void *device_handle, esp_blockdev_handle_t *out_handle)
{
    if (device_handle == NULL || out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_handle = NULL;
    fs_nand_handle_t *handle = device_handle;
    if (handle->cleanup_started || handle->bdl_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    *out_handle = handle->bdl_handle;
    return ESP_OK;
}

bool fs_nand_cleanup_pending(void)
{
    return pending_cleanup != NULL;
}

esp_err_t fs_nand_cleanup_error(void)
{
    return last_cleanup_error;
}

esp_err_t fs_nand_cleanup(void)
{
    if (pending_cleanup == NULL) {
        return ESP_OK;
    }
    last_cleanup_error = cleanup_handle(pending_cleanup);
    return last_cleanup_error;
}

esp_err_t esp_mosaico_nand_init(const esp_mosaico_nand_config_t *config, void **device_handle)
{
#if !CONFIG_NAND_FLASH_ENABLE_BDL
    (void)config;
    (void)device_handle;
    ESP_LOGE(TAG, "NAND BDL support is disabled");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (config == NULL || device_handle == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }
    *device_handle = NULL;
    esp_err_t ret = fs_nand_cleanup();
    if (ret != ESP_OK) {
        return ret;
    }
    last_cleanup_error = ESP_OK;

    const esp_mosaico_nand_config_t *nand_config = config;
    fs_nand_handle_t *handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    handle->peripheral_name = nand_config->peripheral_name;
    handle->hold_gpio_num = nand_config->hold_gpio_num;
    handle->wp_gpio_num = nand_config->wp_gpio_num;
    handle->hold_pin_owned = true;
    handle->wp_pin_owned = true;

    ret = configure_protect_pins(handle->hold_gpio_num, handle->wp_gpio_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to release NAND HOLD/WP: %s", esp_err_to_name(ret));
        goto fail;
    }

    periph_spi_handle_t *spi_bus = NULL;
    ret = brookesia_hal_board_periph_ref_handle(handle->peripheral_name, (void **)&spi_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire NAND SPI bus: %s", esp_err_to_name(ret));
        goto fail;
    }
    handle->peripheral_referenced = true;

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
        goto fail;
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
        goto fail;
    }

    *device_handle = handle;
    return ESP_OK;

fail:
    handle->cleanup_started = true;
    pending_cleanup = handle;
    (void)fs_nand_cleanup();
    return ret;
#endif
}

esp_err_t esp_mosaico_nand_deinit(void *device_handle)
{
    if (device_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    fs_nand_handle_t *handle = device_handle;
    handle->cleanup_started = true;
    /* BM 0.5.15 forgets a custom handle even if deinit fails. Keep its owner here. */
    pending_cleanup = handle;
    return fs_nand_cleanup();
}

static esp_err_t cleanup_handle(fs_nand_handle_t *handle)
{
    if (handle->peripheral_error != ESP_OK) {
        return handle->peripheral_error;
    }

    if (handle->bdl_handle != NULL) {
        esp_blockdev_handle_t bdl_handle = handle->bdl_handle;
        esp_err_t ret = bdl_handle->ops->sync(bdl_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to sync NAND BDL during deinit: %s", esp_err_to_name(ret));
            handle->consumed_error = ret;
        }
        /* BDL release consumes its handle even on error; never call it twice. */
        handle->bdl_handle = NULL;
        ret = bdl_handle->ops->release(bdl_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to release NAND BDL: %s", esp_err_to_name(ret));
            if (handle->consumed_error == ESP_OK) {
                handle->consumed_error = ret;
            }
        }
    }

    if (handle->spi_device != NULL) {
        esp_err_t ret = spi_bus_remove_device(handle->spi_device);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to remove NAND SPI device: %s", esp_err_to_name(ret));
            return ret;
        }
        handle->spi_device = NULL;
    }

    if (handle->peripheral_referenced) {
        /* A failing bus deinit may already have consumed the underlying handle. */
        handle->peripheral_referenced = false;
        esp_err_t ret = brookesia_hal_board_periph_unref_handle(handle->peripheral_name);
        if (ret != ESP_OK) {
            handle->peripheral_error = ret;
            ESP_LOGE(TAG, "NAND SPI release outcome is unknown; restart required: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    if (handle->hold_pin_owned) {
        esp_err_t ret = gpio_reset_pin(handle->hold_gpio_num);
        if (ret != ESP_OK) {
            return ret;
        }
        handle->hold_pin_owned = false;
    }
    if (handle->wp_pin_owned) {
        esp_err_t ret = gpio_reset_pin(handle->wp_gpio_num);
        if (ret != ESP_OK) {
            return ret;
        }
        handle->wp_pin_owned = false;
    }
    const esp_err_t result = handle->consumed_error;
    pending_cleanup = NULL;
    free(handle);
    return result;
}

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
