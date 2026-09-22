/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "driver/i2c_master.h"
#include "esp_bit_defs.h"
#include "esp_err.h"
#include "esp_log.h"
#include "brookesia/hal_adaptor/board_manager.h"
#include "brookesia/hal_custom/board/fuel_gauge.h"

static const char *TAG = "MOSAICO_BQ27220";

enum {
    BQ27220_REG_VOLTAGE = 0x08,
    BQ27220_REG_BATTERY_STATUS = 0x0A,
    BQ27220_BATTERY_STATUS_PRESENT = BIT(3),
    BQ27220_I2C_TIMEOUT_MS = 100,
};

typedef struct {
    i2c_master_dev_handle_t i2c_device;
    const char *peripheral_name;
    bool peripheral_referenced;
    bool cleanup_started;
    esp_err_t peripheral_error;
} bq27220_handle_t;

static bq27220_handle_t *pending_cleanup;
static esp_err_t last_cleanup_error;

static esp_err_t cleanup_handle(bq27220_handle_t *handle);

static esp_err_t bq27220_read_u16(
    i2c_master_dev_handle_t device, uint8_t register_address, uint16_t *value
);

esp_err_t bq27220_fuel_gauge_get_snapshot(
    void *device_handle, bq27220_fuel_gauge_snapshot_t *snapshot
)
{
    if (device_handle == NULL || snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bq27220_handle_t *handle = device_handle;
    if (handle->cleanup_started || handle->i2c_device == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    uint16_t battery_status = 0;
    uint16_t voltage_mv = 0;
    esp_err_t ret = bq27220_read_u16(handle->i2c_device, BQ27220_REG_BATTERY_STATUS, &battery_status);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = bq27220_read_u16(handle->i2c_device, BQ27220_REG_VOLTAGE, &voltage_mv);
    if (ret != ESP_OK) {
        return ret;
    }

    *snapshot = (bq27220_fuel_gauge_snapshot_t) {
        .is_present = (battery_status & BQ27220_BATTERY_STATUS_PRESENT) != 0,
        .has_voltage_mv = (battery_status & BQ27220_BATTERY_STATUS_PRESENT) != 0,
        .voltage_mv = voltage_mv,
    };
    return ESP_OK;
}

bool bq27220_fuel_gauge_cleanup_pending(void)
{
    return pending_cleanup != NULL;
}

esp_err_t bq27220_fuel_gauge_cleanup_error(void)
{
    return last_cleanup_error;
}

esp_err_t bq27220_fuel_gauge_cleanup(void)
{
    if (pending_cleanup == NULL) {
        return ESP_OK;
    }
    last_cleanup_error = cleanup_handle(pending_cleanup);
    return last_cleanup_error;
}

esp_err_t esp_mosaico_fuel_gauge_init(const esp_mosaico_fuel_gauge_config_t *config, void **device_handle)
{
    if (config == NULL || device_handle == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }
    *device_handle = NULL;
    esp_err_t ret = bq27220_fuel_gauge_cleanup();
    if (ret != ESP_OK) {
        return ret;
    }
    last_cleanup_error = ESP_OK;

    const esp_mosaico_fuel_gauge_config_t *fuel_gauge_config = config;
    bq27220_handle_t *handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    handle->peripheral_name = fuel_gauge_config->peripheral_name;

    i2c_master_bus_handle_t i2c_bus = NULL;
    ret = brookesia_hal_board_periph_ref_handle(handle->peripheral_name, (void **)&i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire I2C bus: %s", esp_err_to_name(ret));
        goto fail;
    }
    handle->peripheral_referenced = true;

    const i2c_device_config_t i2c_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = fuel_gauge_config->i2c_addr,
        .scl_speed_hz = fuel_gauge_config->frequency,
    };
    ret = i2c_master_bus_add_device(i2c_bus, &i2c_config, &handle->i2c_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add BQ27220 to I2C bus: %s", esp_err_to_name(ret));
        goto fail;
    }

    bq27220_fuel_gauge_snapshot_t snapshot = {};
    ret = bq27220_fuel_gauge_get_snapshot(handle, &snapshot);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to probe BQ27220: %s", esp_err_to_name(ret));
        goto fail;
    }

    *device_handle = handle;
    return ESP_OK;

fail:
    handle->cleanup_started = true;
    pending_cleanup = handle;
    (void)bq27220_fuel_gauge_cleanup();
    return ret;
}

esp_err_t esp_mosaico_fuel_gauge_deinit(void *device_handle)
{
    if (device_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bq27220_handle_t *handle = device_handle;
    handle->cleanup_started = true;
    /* The BM custom dispatcher consumes its reference even when cleanup fails. */
    pending_cleanup = handle;
    return bq27220_fuel_gauge_cleanup();
}

static esp_err_t cleanup_handle(bq27220_handle_t *handle)
{
    if (handle->peripheral_error != ESP_OK) {
        return handle->peripheral_error;
    }
    if (handle->i2c_device != NULL) {
        esp_err_t ret = i2c_master_bus_rm_device(handle->i2c_device);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "BQ27220 I2C device is still attached: %s", esp_err_to_name(ret));
            return ret;
        }
        handle->i2c_device = NULL;
    }
    if (handle->peripheral_referenced) {
        handle->peripheral_referenced = false;
        esp_err_t ret = brookesia_hal_board_periph_unref_handle(handle->peripheral_name);
        if (ret != ESP_OK) {
            handle->peripheral_error = ret;
            ESP_LOGE(TAG, "BQ27220 I2C release outcome is unknown; restart required: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    pending_cleanup = NULL;
    free(handle);
    return ESP_OK;
}

static esp_err_t bq27220_read_u16(
    i2c_master_dev_handle_t device, uint8_t register_address, uint16_t *value
)
{
    uint8_t data[2] = {};
    esp_err_t ret = i2c_master_transmit_receive(
                        device, &register_address, sizeof(register_address), data, sizeof(data), BQ27220_I2C_TIMEOUT_MS
                    );
    if (ret == ESP_OK) {
        *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    }
    return ret;
}
