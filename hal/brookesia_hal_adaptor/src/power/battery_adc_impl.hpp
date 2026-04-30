/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/hal_interface/interfaces/power/battery.hpp"

namespace esp_brookesia::hal {

class BatteryAdcImpl : public PowerBatteryIface {
public:
    BatteryAdcImpl();
    ~BatteryAdcImpl() override;

    bool get_state(State &state) override;
    bool get_charge_config(ChargeConfig &config) override;
    bool set_charge_config(const ChargeConfig &config) override;
    bool set_charging_enabled(bool enabled) override;

    bool is_valid() const
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        return is_voltage_adc_valid_internal();
    }

private:
    enum class CalibrationScheme {
        None,
        CurveFitting,
        LineFitting,
    };

    bool setup_voltage_adc();
    bool setup_charge_adc();
    bool read_voltage_adc_mv(uint32_t &voltage_mv);
    bool read_charge_adc_mv(uint32_t &voltage_mv);
    bool read_adc_mv(void *adc_handle, void *adc_config, void *adc_cali_handle, uint32_t &voltage_mv);
    bool create_adc_calibration(void *adc_config, void **adc_cali_handle, CalibrationScheme &scheme);
    void delete_adc_calibration(void *adc_cali_handle, CalibrationScheme scheme);

    bool is_voltage_adc_valid_internal() const
    {
        return (voltage_adc_handle_ != nullptr) && (voltage_adc_config_ != nullptr);
    }

    bool is_charge_adc_valid_internal() const
    {
        return (charge_adc_handle_ != nullptr) && (charge_adc_config_ != nullptr);
    }

    mutable boost::mutex mutex_;

    void *voltage_adc_handle_ = nullptr;
    void *voltage_adc_config_ = nullptr;
    void *voltage_adc_cali_handle_ = nullptr;
    CalibrationScheme voltage_adc_cali_scheme_ = CalibrationScheme::None;

    void *charge_adc_handle_ = nullptr;
    void *charge_adc_config_ = nullptr;
    void *charge_adc_cali_handle_ = nullptr;
    CalibrationScheme charge_adc_cali_scheme_ = CalibrationScheme::None;
};

} // namespace esp_brookesia::hal
