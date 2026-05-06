/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/hal_interface/interfaces/power/battery.hpp"

namespace esp_brookesia::hal {

class BatteryAxp2101Impl : public PowerBatteryIface {
public:
    BatteryAxp2101Impl();
    ~BatteryAxp2101Impl() override;

    bool get_state(State &state) override;
    bool get_charge_config(ChargeConfig &config) override;
    bool set_charge_config(const ChargeConfig &config) override;
    bool set_charging_enabled(bool enabled) override;

    bool is_valid() const
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        return is_valid_internal();
    }

private:
    bool setup_power_manager();

    bool is_valid_internal() const
    {
        return device_handle_ != nullptr;
    }

    mutable boost::mutex mutex_;
    void *device_handle_ = nullptr;
    bool device_initialized_ = false;
};

} // namespace esp_brookesia::hal
