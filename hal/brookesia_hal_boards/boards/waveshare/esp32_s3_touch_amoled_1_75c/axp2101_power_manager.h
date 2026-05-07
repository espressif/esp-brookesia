/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef enum {
    POWER_MANAGER_BATTERY_POWER_SOURCE_UNKNOWN,
    POWER_MANAGER_BATTERY_POWER_SOURCE_BATTERY,
    POWER_MANAGER_BATTERY_POWER_SOURCE_EXTERNAL,
} power_manager_battery_power_source_t;

typedef enum {
    POWER_MANAGER_BATTERY_CHARGE_STATE_UNKNOWN,
    POWER_MANAGER_BATTERY_CHARGE_STATE_NOT_CHARGING,
    POWER_MANAGER_BATTERY_CHARGE_STATE_CHARGING,
    POWER_MANAGER_BATTERY_CHARGE_STATE_TRICKLE,
    POWER_MANAGER_BATTERY_CHARGE_STATE_PRE_CHARGE,
    POWER_MANAGER_BATTERY_CHARGE_STATE_CONSTANT_CURRENT,
    POWER_MANAGER_BATTERY_CHARGE_STATE_CONSTANT_VOLTAGE,
    POWER_MANAGER_BATTERY_CHARGE_STATE_FULL,
    POWER_MANAGER_BATTERY_CHARGE_STATE_FAULT,
} power_manager_battery_charge_state_t;

typedef struct {
    bool is_present;
    power_manager_battery_power_source_t power_source;
    power_manager_battery_charge_state_t charge_state;
    bool has_voltage_mv;
    uint32_t voltage_mv;
    bool has_percentage;
    uint8_t percentage;
    bool has_vbus_voltage_mv;
    uint32_t vbus_voltage_mv;
    bool has_system_voltage_mv;
    uint32_t system_voltage_mv;
} power_manager_battery_state_t;

typedef struct {
    bool enabled;
    uint32_t target_voltage_mv;
    uint32_t charge_current_ma;
    uint32_t precharge_current_ma;
    uint32_t termination_current_ma;
} power_manager_battery_charge_config_t;

esp_err_t power_manager_get_battery_state(void *device_handle, power_manager_battery_state_t *state);

esp_err_t power_manager_get_charge_config(void *device_handle, power_manager_battery_charge_config_t *config);

esp_err_t power_manager_set_charge_config(void *device_handle, const power_manager_battery_charge_config_t *config);

esp_err_t power_manager_set_charging_enabled(void *device_handle, bool enabled);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
