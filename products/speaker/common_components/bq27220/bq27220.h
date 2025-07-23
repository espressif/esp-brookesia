/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_assert.h"
#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BQ27220_ERROR 0x0
#define BQ27220_SUCCESS 0x1

typedef union {
    struct {
        // Low byte, Low bit first
        bool DSG : 1; // The device is in DISCHARGE
        bool SYSDWN : 1; // System down bit indicating the system should shut down
        bool TDA : 1; // Terminate Discharge Alarm
        bool BATTPRES : 1; // Battery Present detected
        bool AUTH_GD : 1; // Detect inserted battery
        bool OCVGD : 1; // Good OCV measurement taken
        bool TCA : 1; // Terminate Charge Alarm
        bool RSVD : 1; // Reserved
        // High byte, Low bit first
        bool CHGINH : 1; // Charge inhibit
        bool FC : 1; // Full-charged is detected
        bool OTD : 1; // Overtemperature in discharge condition is detected
        bool OTC : 1; // Overtemperature in charge condition is detected
        bool SLEEP : 1; // Device is operating in SLEEP mode when set
        bool OCVFAIL : 1; // Status bit indicating that the OCV reading failed due to current
        bool OCVCOMP : 1; // An OCV measurement update is complete
        bool FD : 1; // Full-discharge is detected
    };
    uint16_t full;
} BatteryStatus;

ESP_STATIC_ASSERT(sizeof(BatteryStatus) == 2, "Incorrect structure size");

typedef struct {
    // Low byte, Low bit first
    bool CALMD : 1; /**< Calibration mode enabled */
    uint8_t SEC : 2; /**< Current security access */
    bool EDV2 : 1; /**< EDV2 threshold exceeded */
    bool VDQ : 1; /**< Indicates if Current discharge cycle is NOT qualified or qualified for an FCC updated */
    bool INITCOMP : 1; /**< gauge initialization is complete */
    bool SMTH : 1; /**< RemainingCapacity is scaled by smooth engine */
    bool BTPINT : 1; /**< BTP threshold has been crossed */
    // High byte, Low bit first
    uint8_t RSVD1 : 2;
    bool CFGUPDATE : 1; /**< Gauge is in CONFIG UPDATE mode */
    uint8_t RSVD0 : 5;
} OperationStatus;

ESP_STATIC_ASSERT(sizeof(OperationStatus) == 2, "Incorrect structure size");

typedef struct {
    // Low byte, Low bit first
    bool CCT : 1; // 0 = Use CC % of DesignCapacity() (default), 1 = Use CC % of FullChargeCapacity()
    bool CSYNC : 1; // Sync RemainingCapacity() with FullChargeCapacity() at valid charge termination
    bool RSVD0 : 1;
    bool EDV_CMP : 1;
    bool SC : 1;
    bool FIXED_EDV0 : 1;
    uint8_t RSVD1 : 2;
    // High byte, Low bit first
    bool FCC_LIM : 1;
    bool RSVD2 : 1;
    bool FC_FOR_VDQ : 1; // full charge voltage for VDQ
    bool IGNORE_SD : 1;  // ignore self-discharge
    bool SME0 : 1;
    uint8_t RSVD3 : 3;
} GaugingConfig;

ESP_STATIC_ASSERT(sizeof(GaugingConfig) == 2, "Incorrect structure size");

typedef struct {
    union {
        GaugingConfig gauge_conf;
        uint16_t gauge_conf_raw;
    } cedv_conf;
    uint16_t full_charge_cap;
    uint16_t design_cap;
    uint16_t reserve_cap;
    uint16_t near_full;
    uint16_t self_discharge_rate;
    uint16_t EDV0;
    uint16_t EDV1;
    uint16_t EDV2;
    uint16_t EMF;
    uint16_t C0;
    uint16_t R0;
    uint16_t T0;
    uint16_t R1;
    uint8_t TC;
    uint8_t C1;
    uint16_t DOD0;
    uint16_t DOD10;
    uint16_t DOD20;
    uint16_t DOD30;
    uint16_t DOD40;
    uint16_t DOD50;
    uint16_t DOD60;
    uint16_t DOD70;
    uint16_t DOD80;
    uint16_t DOD90;
    uint16_t DOD100;
} ParamCEDV;


typedef struct {
    i2c_bus_handle_t i2c_bus;  // I2C bus handle
    ParamCEDV *cedv;  // Pointer to the CEDV structure
} bq27220_config_t;


typedef void *bq27220_handle_t;

/** Initialize Driver
 * @return true on success, false otherwise
 */
bq27220_handle_t bq27220_init(const bq27220_config_t *config);

bool bq27220_deinit(bq27220_handle_t bq_handle);

uint16_t bq27220_get_fw_version(bq27220_handle_t bq_handle);

uint16_t bq27220_get_hw_version(bq27220_handle_t bq_handle);

/** Get battery voltage in mV or error */
uint16_t bq27220_get_voltage(bq27220_handle_t handle);

/** Get current in mA or error*/
int16_t bq27220_get_current(bq27220_handle_t handle);

int16_t bq27220_get_avgcurrent(bq27220_handle_t bq_handle);

uint16_t bq27220_get_cycle_count(bq27220_handle_t bq_handle);

/** Get battery status */
uint8_t bq27220_get_battery_status(bq27220_handle_t handle, BatteryStatus *battery_status);

/** Get operation status */
uint8_t bq27220_get_operation_status(bq27220_handle_t handle, OperationStatus *operation_status);

/** Get temperature in units of 0.1°K */
uint16_t bq27220_get_temperature(bq27220_handle_t handle);

/** Get compensated full charge capacity in in mAh */
uint16_t bq27220_get_full_charge_capacity(bq27220_handle_t handle);

/** Get design capacity in mAh */
uint16_t bq27220_get_design_capacity(bq27220_handle_t handle);

/** Get remaining capacity in in mAh */
uint16_t bq27220_get_remaining_capacity(bq27220_handle_t handle);

/** Get predicted remaining battery capacity in percents */
uint16_t bq27220_get_state_of_charge(bq27220_handle_t handle);

/** Get ratio of full charge capacity over design capacity in percents */
uint16_t bq27220_get_state_of_health(bq27220_handle_t handle);

uint16_t bq27220_get_charge_voltage(bq27220_handle_t bq_handle);

uint16_t bq27220_get_charge_current(bq27220_handle_t bq_handle);

int16_t bq27220_get_average_power(bq27220_handle_t bq_handle);

uint16_t bq27220_get_time_to_empty(bq27220_handle_t bq_handle);

uint16_t bq27220_get_time_to_full(bq27220_handle_t bq_handle);

int16_t bq27220_get_maxload_current(bq27220_handle_t bq_handle);

int16_t bq27220_get_standby_current(bq27220_handle_t bq_handle);


#ifdef __cplusplus
}
#endif
