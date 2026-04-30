/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/hal_interface/interfaces/power/battery.hpp"
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia {

class TestPowerBatteryIface: public hal::PowerBatteryIface {
public:
    static constexpr const char *NAME = "TestPowerBattery:Battery";

    explicit TestPowerBatteryIface(bool charger_control_supported);
    ~TestPowerBatteryIface() = default;

    bool get_state(hal::PowerBatteryIface::State &state) override;
    bool get_charge_config(hal::PowerBatteryIface::ChargeConfig &config) override;
    bool set_charge_config(const hal::PowerBatteryIface::ChargeConfig &config) override;
    bool set_charging_enabled(bool enabled) override;

private:
    bool charger_control_supported_ = false;
    hal::PowerBatteryIface::State state_{};
    hal::PowerBatteryIface::ChargeConfig charge_config_{};
};

class TestBatteryDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestPowerBattery";

    TestBatteryDevice()
        : hal::Device(std::string(NAME))
    {
    }

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    static void set_charger_control_supported(bool charger_control_supported);
};

} // namespace esp_brookesia
