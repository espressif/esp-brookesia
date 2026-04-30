#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_bit_defs.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "esp_board_device.h"
#include "esp_board_periph.h"
#include "gen_board_device_custom.h"
#include "power_manager.h"

static const char *TAG = "CUSTOM_POWER_MANAGER";

enum {
    AXP2101_REG_STATUS1 = 0x00,
    AXP2101_REG_STATUS2 = 0x01,
    AXP2101_REG_CHARGER_FUEL_GAUGE_CONTROL = 0x18,
    AXP2101_REG_ADC_CHANNEL_ENABLE = 0x30,
    AXP2101_REG_VBAT_H = 0x34,
    AXP2101_REG_VBUS_H = 0x38,
    AXP2101_REG_VSYS_H = 0x3A,
    AXP2101_REG_IPRECHG_CHG_SET = 0x61,
    AXP2101_REG_ICC_CHG_SET = 0x62,
    AXP2101_REG_ITERM_CHG_SET = 0x63,
    AXP2101_REG_CV_CHG_SET = 0x64,
    AXP2101_REG_BATTERY_PERCENTAGE = 0xA4,
};

static esp_err_t axp2101_read_reg(i2c_master_dev_handle_t handle, uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(handle, &reg, sizeof(reg), value, sizeof(*value), 1000);
}

static esp_err_t axp2101_write_reg(i2c_master_dev_handle_t handle, uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(handle, data, sizeof(data), 1000);
}

static esp_err_t axp2101_update_reg_bits(i2c_master_dev_handle_t handle, uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t current = 0;
    ESP_RETURN_ON_ERROR(axp2101_read_reg(handle, reg, &current), TAG, "Failed to read AXP2101 reg 0x%02x", reg);
    current = (current & ~mask) | (value & mask);
    return axp2101_write_reg(handle, reg, current);
}

static esp_err_t axp2101_read_u14(i2c_master_dev_handle_t handle, uint8_t high_reg, uint32_t *value)
{
    uint8_t data[2] = {};
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit_receive(handle, &high_reg, sizeof(high_reg), data, sizeof(data), 1000), TAG,
        "Failed to read AXP2101 ADC reg 0x%02x", high_reg
    );
    *value = ((uint32_t)(data[0] & 0x3f) << 8) | data[1];
    return ESP_OK;
}

static power_manager_battery_charge_state_t convert_charge_state(uint8_t status)
{
    switch (status & 0x07) {
    case 0:
        return POWER_MANAGER_BATTERY_CHARGE_STATE_TRICKLE;
    case 1:
        return POWER_MANAGER_BATTERY_CHARGE_STATE_PRE_CHARGE;
    case 2:
        return POWER_MANAGER_BATTERY_CHARGE_STATE_CONSTANT_CURRENT;
    case 3:
        return POWER_MANAGER_BATTERY_CHARGE_STATE_CONSTANT_VOLTAGE;
    case 4:
        return POWER_MANAGER_BATTERY_CHARGE_STATE_FULL;
    case 5:
        return POWER_MANAGER_BATTERY_CHARGE_STATE_NOT_CHARGING;
    case 6:
    default:
        return POWER_MANAGER_BATTERY_CHARGE_STATE_UNKNOWN;
    }
}

static uint8_t encode_charge_current(uint32_t current_ma)
{
    static const uint16_t supported_ma[] = {0, 100, 125, 150, 175, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
    static const uint8_t supported_reg[] = {0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    size_t best = 0;
    uint32_t best_delta = UINT32_MAX;
    for (size_t i = 0; i < sizeof(supported_ma) / sizeof(supported_ma[0]); i++) {
        uint32_t delta = (current_ma > supported_ma[i]) ? (current_ma - supported_ma[i]) : (supported_ma[i] - current_ma);
        if (delta < best_delta) {
            best = i;
            best_delta = delta;
        }
    }
    return supported_reg[best];
}

static uint32_t decode_charge_current(uint8_t reg_value)
{
    uint8_t code = reg_value & 0x1f;
    if (code <= 8) {
        return code * 25;
    }
    if (code <= 16) {
        return 200 + (code - 8) * 100;
    }
    return 0;
}

static uint8_t encode_25ma_step_current(uint32_t current_ma)
{
    uint32_t code = (current_ma + 12) / 25;
    return (uint8_t)(code > 8 ? 8 : code);
}

static uint8_t encode_target_voltage(uint32_t target_voltage_mv)
{
    static const uint16_t supported_mv[] = {4000, 4100, 4200, 4350, 4400};
    static const uint8_t supported_reg[] = {1, 2, 3, 4, 5};
    size_t best = 0;
    uint32_t best_delta = UINT32_MAX;
    for (size_t i = 0; i < sizeof(supported_mv) / sizeof(supported_mv[0]); i++) {
        uint32_t delta = (target_voltage_mv > supported_mv[i]) ?
                         (target_voltage_mv - supported_mv[i]) : (supported_mv[i] - target_voltage_mv);
        if (delta < best_delta) {
            best = i;
            best_delta = delta;
        }
    }
    return supported_reg[best];
}

static uint32_t decode_target_voltage(uint8_t reg_value)
{
    switch (reg_value & 0x07) {
    case 1:
        return 4000;
    case 2:
        return 4100;
    case 3:
        return 4200;
    case 4:
        return 4350;
    case 5:
        return 4400;
    default:
        return 0;
    }
}

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

esp_err_t power_manager_get_battery_state(void *device_handle, power_manager_battery_state_t *state)
{
    ESP_RETURN_ON_FALSE(device_handle != NULL && state != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    i2c_master_dev_handle_t axp2101_h = ((power_manager_handle_t *)device_handle)->pm_handle;
    uint8_t status1 = 0;
    uint8_t status2 = 0;
    ESP_RETURN_ON_ERROR(axp2101_read_reg(axp2101_h, AXP2101_REG_STATUS1, &status1), TAG, "Failed to read status1");
    ESP_RETURN_ON_ERROR(axp2101_read_reg(axp2101_h, AXP2101_REG_STATUS2, &status2), TAG, "Failed to read status2");

    *state = (power_manager_battery_state_t) {
        .is_present = (status1 & BIT(3)) != 0,
        .power_source = (status1 & BIT(5)) != 0 ? POWER_MANAGER_BATTERY_POWER_SOURCE_EXTERNAL :
                        POWER_MANAGER_BATTERY_POWER_SOURCE_BATTERY,
        .charge_state = convert_charge_state(status2),
    };

    ESP_RETURN_ON_ERROR(
        axp2101_update_reg_bits(axp2101_h, AXP2101_REG_ADC_CHANNEL_ENABLE, BIT(0) | BIT(2) | BIT(3),
                                BIT(0) | BIT(2) | BIT(3)),
        TAG, "Failed to enable ADC channels"
    );

    uint32_t voltage_mv = 0;
    if (state->is_present && axp2101_read_u14(axp2101_h, AXP2101_REG_VBAT_H, &voltage_mv) == ESP_OK) {
        state->has_voltage_mv = true;
        state->voltage_mv = voltage_mv;
    }
    if (axp2101_read_u14(axp2101_h, AXP2101_REG_VBUS_H, &voltage_mv) == ESP_OK) {
        state->has_vbus_voltage_mv = true;
        state->vbus_voltage_mv = voltage_mv;
    }
    if (axp2101_read_u14(axp2101_h, AXP2101_REG_VSYS_H, &voltage_mv) == ESP_OK) {
        state->has_system_voltage_mv = true;
        state->system_voltage_mv = voltage_mv;
    }

    uint8_t percentage = 0;
    if (state->is_present && axp2101_read_reg(axp2101_h, AXP2101_REG_BATTERY_PERCENTAGE, &percentage) == ESP_OK &&
            percentage <= 100) {
        state->has_percentage = true;
        state->percentage = percentage;
    }

    return ESP_OK;
}

esp_err_t power_manager_get_charge_config(void *device_handle, power_manager_battery_charge_config_t *config)
{
    ESP_RETURN_ON_FALSE(device_handle != NULL && config != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    i2c_master_dev_handle_t axp2101_h = ((power_manager_handle_t *)device_handle)->pm_handle;
    uint8_t charger_control = 0;
    uint8_t precharge = 0;
    uint8_t charge = 0;
    uint8_t termination = 0;
    uint8_t target_voltage = 0;

    ESP_RETURN_ON_ERROR(
        axp2101_read_reg(axp2101_h, AXP2101_REG_CHARGER_FUEL_GAUGE_CONTROL, &charger_control), TAG,
        "Failed to read charger control"
    );
    ESP_RETURN_ON_ERROR(axp2101_read_reg(axp2101_h, AXP2101_REG_IPRECHG_CHG_SET, &precharge), TAG, "Failed to read precharge");
    ESP_RETURN_ON_ERROR(axp2101_read_reg(axp2101_h, AXP2101_REG_ICC_CHG_SET, &charge), TAG, "Failed to read charge current");
    ESP_RETURN_ON_ERROR(
        axp2101_read_reg(axp2101_h, AXP2101_REG_ITERM_CHG_SET, &termination), TAG, "Failed to read termination"
    );
    ESP_RETURN_ON_ERROR(
        axp2101_read_reg(axp2101_h, AXP2101_REG_CV_CHG_SET, &target_voltage), TAG, "Failed to read target voltage"
    );

    *config = (power_manager_battery_charge_config_t) {
        .enabled = (charger_control & BIT(1)) != 0,
        .target_voltage_mv = decode_target_voltage(target_voltage),
        .charge_current_ma = decode_charge_current(charge),
        .precharge_current_ma = (precharge & 0x0f) * 25,
        .termination_current_ma = (termination & 0x0f) * 25,
    };

    return ESP_OK;
}

esp_err_t power_manager_set_charge_config(void *device_handle, const power_manager_battery_charge_config_t *config)
{
    ESP_RETURN_ON_FALSE(device_handle != NULL && config != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    i2c_master_dev_handle_t axp2101_h = ((power_manager_handle_t *)device_handle)->pm_handle;
    ESP_RETURN_ON_ERROR(
        power_manager_set_charging_enabled(device_handle, config->enabled), TAG, "Failed to set charger enable"
    );
    ESP_RETURN_ON_ERROR(
        axp2101_write_reg(axp2101_h, AXP2101_REG_CV_CHG_SET, encode_target_voltage(config->target_voltage_mv)),
        TAG, "Failed to set target voltage"
    );
    ESP_RETURN_ON_ERROR(
        axp2101_write_reg(axp2101_h, AXP2101_REG_IPRECHG_CHG_SET, encode_25ma_step_current(config->precharge_current_ma)),
        TAG, "Failed to set precharge current"
    );
    ESP_RETURN_ON_ERROR(
        axp2101_write_reg(axp2101_h, AXP2101_REG_ICC_CHG_SET, encode_charge_current(config->charge_current_ma)),
        TAG, "Failed to set charge current"
    );

    uint8_t termination = 0;
    ESP_RETURN_ON_ERROR(
        axp2101_read_reg(axp2101_h, AXP2101_REG_ITERM_CHG_SET, &termination), TAG, "Failed to read termination"
    );
    termination = (termination & 0xf0) | encode_25ma_step_current(config->termination_current_ma);
    ESP_RETURN_ON_ERROR(
        axp2101_write_reg(axp2101_h, AXP2101_REG_ITERM_CHG_SET, termination), TAG, "Failed to set termination"
    );

    return ESP_OK;
}

esp_err_t power_manager_set_charging_enabled(void *device_handle, bool enabled)
{
    ESP_RETURN_ON_FALSE(device_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    i2c_master_dev_handle_t axp2101_h = ((power_manager_handle_t *)device_handle)->pm_handle;
    return axp2101_update_reg_bits(
               axp2101_h, AXP2101_REG_CHARGER_FUEL_GAUGE_CONTROL, BIT(1), enabled ? BIT(1) : 0
           );
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
        goto cleanup;
    }
    // hold 4s to power off
    data[0] = 0x27;
    data[1] = 0x10;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write hold 4s to power off");
        goto cleanup;
    }
    // Disable All DCs but DC1
    data[0] = 0x80;
    data[1] = 0x01;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write Disable All DCs but DC1");
        goto cleanup;
    }
    // Disable All LDOs
    data[0] = 0x90;
    data[1] = 0x00;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write Disable All LDOs");
        goto cleanup;
    }
    // Set DC1 to 3.3V
    data[0] = 0x82;
    data[1] = (3300 - 1500) / 100;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write Set DC1 to 3.3V");
        goto cleanup;
    }
    // Set ALDO1 to 3.3V
    data[0] = 0x92;
    data[1] = (3300 - 500) / 100;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write Set ALDO1 to 3.3V");
        goto cleanup;
    }

    power_manager_enable(handle, POWER_MANAGER_FEATURE_MIC);

    // CV charger voltage setting to 4.1V
    data[0] = 0x64;
    data[1] = 0x02;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write CV charger voltage setting to 4.1V");
        goto cleanup;
    }
    // set Main battery precharge current to 50mA
    data[0] = 0x61;
    data[1] = 0x02;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write set Main battery precharge current to 50mA");
        goto cleanup;
    }
    // set Main battery charger current to 400mA
    data[0] = 0x62;
    data[1] = 0x08;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write set Main battery charger current to 400mA");
        goto cleanup;
    }
    // set Main battery term charge current to 25mA
    data[0] = 0x63;
    data[1] = 0x01;
    err = i2c_master_transmit(handle->pm_handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write set Main battery term charge current to 25mA");
        goto cleanup;
    }

    *device_handle = handle;

    return ESP_OK;

cleanup:
    i2c_master_bus_rm_device(handle->pm_handle);
    free(handle);
    return err;
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
