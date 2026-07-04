/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/hal_interface/interfaces/power/battery.hpp"
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia {

class TestPowerBatteryIface: public hal::power::BatteryIface {
public:
    static constexpr const char *NAME = "TestPowerBattery:Battery";

    explicit TestPowerBatteryIface(bool charger_control_supported);
    ~TestPowerBatteryIface() = default;

    bool get_state(hal::power::BatteryIface::State &state) override;
    bool get_charge_config(hal::power::BatteryIface::ChargeConfig &config) override;
    bool set_charge_config(const hal::power::BatteryIface::ChargeConfig &config) override;
    bool set_charging_enabled(bool enabled) override;

private:
    bool charger_control_supported_ = false;
    hal::power::BatteryIface::State state_{};
    hal::power::BatteryIface::ChargeConfig charge_config_{};
};

class TestBatteryDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestPowerBattery";

    TestBatteryDevice()
        : hal::Device(std::string(NAME))
    {
    }

    bool probe() override;
    std::vector<hal::InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    static void set_charger_control_supported(bool charger_control_supported);
};

} // namespace esp_brookesia
