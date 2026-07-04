/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.hpp"
#include "test_power_battery.hpp"

using namespace esp_brookesia;

namespace esp_brookesia {

namespace {
bool g_charger_control_supported = true;

hal::power::BatteryIface::Info make_battery_info(bool charger_control_supported)
{
    hal::power::BatteryIface::Info info = {
        .name = "Test battery",
        .chemistry = "Li-ion",
        .abilities = {
            hal::power::BatteryIface::Ability::Voltage,
            hal::power::BatteryIface::Ability::Percentage,
            hal::power::BatteryIface::Ability::PowerSource,
            hal::power::BatteryIface::Ability::ChargeState,
            hal::power::BatteryIface::Ability::VbusVoltage,
        },
    };

    if (charger_control_supported) {
        info.abilities.push_back(hal::power::BatteryIface::Ability::ChargerControl);
        info.abilities.push_back(hal::power::BatteryIface::Ability::ChargeConfig);
    }

    return info;
}
} // namespace

TestPowerBatteryIface::TestPowerBatteryIface(bool charger_control_supported)
    : hal::power::BatteryIface(make_battery_info(charger_control_supported))
    , charger_control_supported_(charger_control_supported)
{
    state_ = {
        .is_present = true,
        .power_source = hal::power::BatteryIface::PowerSource::External,
        .charge_state = hal::power::BatteryIface::ChargeState::ConstantCurrent,
        .level_source = hal::power::BatteryIface::LevelSource::FuelGauge,
        .voltage_mv = 3850,
        .percentage = 67,
        .vbus_voltage_mv = 5000,
        .system_voltage_mv = std::nullopt,
        .is_low = false,
        .is_critical = false,
    };
    charge_config_ = {
        .enabled = true,
        .target_voltage_mv = 4100,
        .charge_current_ma = 400,
        .precharge_current_ma = 50,
        .termination_current_ma = 25,
    };
}

bool TestPowerBatteryIface::get_state(hal::power::BatteryIface::State &state)
{
    state = state_;

    return true;
}

bool TestPowerBatteryIface::get_charge_config(hal::power::BatteryIface::ChargeConfig &config)
{
    if (!charger_control_supported_) {
        return false;
    }

    config = charge_config_;

    return true;
}

bool TestPowerBatteryIface::set_charge_config(const hal::power::BatteryIface::ChargeConfig &config)
{
    if (!charger_control_supported_) {
        return false;
    }

    charge_config_ = config;

    return true;
}

bool TestPowerBatteryIface::set_charging_enabled(bool enabled)
{
    if (!charger_control_supported_) {
        return false;
    }

    charge_config_.enabled = enabled;

    return true;
}

bool TestBatteryDevice::probe()
{
    return true;
}

std::vector<hal::InterfaceSpec> TestBatteryDevice::get_interface_specs() const
{
    return {{hal::power::BatteryIface::NAME, TestPowerBatteryIface::NAME}};
}

bool TestBatteryDevice::on_init()
{
    auto interface = std::make_shared<TestPowerBatteryIface>(g_charger_control_supported);

    interfaces_.emplace(TestPowerBatteryIface::NAME, interface);

    return true;
}

void TestBatteryDevice::on_deinit()
{
    interfaces_.clear();
}

void TestBatteryDevice::set_charger_control_supported(bool charger_control_supported)
{
    g_charger_control_supported = charger_control_supported;
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestBatteryDevice, std::string(TestBatteryDevice::NAME));


TEST_CASE("power::BatteryIface: acquire and read battery state", "[hal][interface]")
{
    TestBatteryDevice::set_charger_control_supported(true);
    auto handle = hal::acquire_first_interface<hal::power::BatteryIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_EQUAL_STRING(TestPowerBatteryIface::NAME, std::string(handle.instance_name()).c_str());

    auto iface = handle.get();
    TEST_ASSERT_NOT_NULL(iface.get());
    const auto &info = iface->get_info();
    TEST_ASSERT_EQUAL_STRING("Test battery", info.name.c_str());

    hal::power::BatteryIface::State state;
    TEST_ASSERT_TRUE(iface->get_state(state));
    TEST_ASSERT_TRUE(state.is_present);
    TEST_ASSERT_EQUAL_UINT32(3850, state.voltage_mv.value_or(0));
    TEST_ASSERT_EQUAL_UINT8(67, state.percentage.value_or(0));

    hal::power::BatteryIface::ChargeConfig config;
    TEST_ASSERT_TRUE(iface->get_charge_config(config));
    config.enabled = false;
    TEST_ASSERT_TRUE(iface->set_charge_config(config));
    TEST_ASSERT_TRUE(iface->set_charging_enabled(true));
}

TEST_CASE("power::BatteryIface: acquire by instance and unsupported charger path", "[hal][interface]")
{
    TestBatteryDevice::set_charger_control_supported(false);
    auto handle = hal::acquire_interface<hal::power::BatteryIface>(TestPowerBatteryIface::NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(handle));

    auto iface = handle.get();
    hal::power::BatteryIface::ChargeConfig config;
    TEST_ASSERT_FALSE(iface->get_charge_config(config));
    TEST_ASSERT_FALSE(iface->set_charging_enabled(false));

    auto handles = hal::acquire_interfaces<hal::power::BatteryIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, handles.size());
}
