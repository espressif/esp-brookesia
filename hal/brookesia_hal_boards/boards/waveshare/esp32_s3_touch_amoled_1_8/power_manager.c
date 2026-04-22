#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "esp_board_device.h"
#include "esp_board_periph.h"
#include "gen_board_device_custom.h"
#include "esp_io_expander.h"
#include "power_manager.h"

static const char *TAG = "CUSTOM_POWER_MANAGER";

esp_err_t power_manager_enable(void *device_handle, power_manager_feature_t feature)
{
    i2c_master_dev_handle_t axp2101_h = ((power_manager_handle_t *)device_handle)->pm_handle;
    esp_err_t err = ESP_OK;
    switch (feature) {
        case POWER_MANAGER_FEATURE_MIC: {
            const uint8_t mic_en[] = {0x90, 0x01};  // Enable ALDO1(MIC)
            err = i2c_master_transmit(axp2101_h, mic_en, sizeof(mic_en), 1000);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write Enable ALDO1(MIC)");
            }
            break;
        }
        default:
            ESP_LOGE(TAG, "Unsupported feature");
            return ESP_ERR_INVALID_ARG;
    }

    return err;
}

int power_manager_init(void *config, int cfg_size, void **device_handle)
{
    ESP_LOGI(TAG, "Initializing power_manager device");
    dev_custom_axp2101_power_manager_config_t *power_manager_cfg = (dev_custom_axp2101_power_manager_config_t *)config;

    if (strcmp(power_manager_cfg->chip, "axp2101") != 0) {
        ESP_LOGE(TAG, "Unsupported power_manager chip: %s", power_manager_cfg->chip);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;

    err = esp_board_periph_init(power_manager_cfg->peripheral_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init power_manager peripheral");
        return err;
    }

    i2c_master_bus_handle_t i2c_master_handle = NULL;
    err = esp_board_periph_get_handle(power_manager_cfg->peripheral_name, (void **)&i2c_master_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get i2c handle");
        return err;
    }

    power_manager_handle_t *handle = calloc(1, sizeof(power_manager_handle_t));
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate power_manager handle");
        return ESP_ERR_NO_MEM;
    }

    const i2c_device_config_t axp2101_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = power_manager_cfg->i2c_addr,
        .scl_speed_hz = power_manager_cfg->frequency,
    };
    err = i2c_master_bus_add_device(i2c_master_handle, &axp2101_config, &handle->pm_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add AXP2101 device to I2C bus");
        free(handle);
        return err;
    }

    uint8_t data[2];

    // PWRON > OFFLEVEL as POWEROFF Source enable
    data[0] = 0x22;
    data[1] = 0b110;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write PWRON > OFFLEVEL as POWEROFF Source enable");
        return err;
    }
    // hold 4s to power off
    data[0] = 0x27;
    data[1] = 0x10;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write hold 4s to power off");
        return err;
    }
    // Disable All DCs but DC1
    data[0] = 0x80;
    data[1] = 0x01;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write Disable All DCs but DC1");
        return err;
    }
    // Disable All LDOs
    data[0] = 0x90;
    data[1] = 0x00;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write Disable All LDOs");
        return err;
    }
    // Set DC1 to 3.3V
    data[0] = 0x82;
    data[1] = (3300 - 1500) / 100;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write Set DC1 to 3.3V");
        return err;
    }
    // Set ALDO1 to 3.3V
    data[0] = 0x92;
    data[1] = (3300 - 500) / 100;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write Set ALDO1 to 3.3V");
        return err;
    }

    power_manager_enable(handle, POWER_MANAGER_FEATURE_MIC);

    // CV charger voltage setting to 4.1V
    data[0] = 0x64;
    data[1] = 0x02;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write CV charger voltage setting to 4.1V");
        return err;
    }
    // set Main battery precharge current to 50mA
    data[0] = 0x61;
    data[1] = 0x02;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write set Main battery precharge current to 50mA");
        return err;
    }
    // set Main battery charger current to 400mA
    data[0] = 0x62;
    data[1] = 0x08;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write set Main battery charger current to 400mA");
        return err;
    }
    // set Main battery term charge current to 25mA
    data[0] = 0x63;
    data[1] = 0x01;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write set Main battery term charge current to 25mA");
        return err;
    }

    *device_handle = handle;

    return ESP_OK;
}

int power_manager_deinit(void *device_handle)
{
    if (device_handle == NULL) {
        ESP_LOGW(TAG, "Power manager device handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    power_manager_handle_t *handle = (power_manager_handle_t *)device_handle;
    esp_err_t err = i2c_master_bus_rm_device(handle->pm_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to remove AXP2101 device from I2C bus");
    }
    free(handle);
    return ESP_OK;
}

CUSTOM_DEVICE_IMPLEMENT(axp2101_power_manager, power_manager_init, power_manager_deinit);
