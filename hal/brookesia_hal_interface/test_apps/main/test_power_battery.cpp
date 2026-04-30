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

hal::PowerBatteryIface::Info make_battery_info(bool charger_control_supported)
{
    hal::PowerBatteryIface::Info info = {
        .name = "Test battery",
        .chemistry = "Li-ion",
        .abilities = {
            hal::PowerBatteryIface::Ability::Voltage,
            hal::PowerBatteryIface::Ability::Percentage,
            hal::PowerBatteryIface::Ability::PowerSource,
            hal::PowerBatteryIface::Ability::ChargeState,
            hal::PowerBatteryIface::Ability::VbusVoltage,
        },
    };

    if (charger_control_supported) {
        info.abilities.push_back(hal::PowerBatteryIface::Ability::ChargerControl);
        info.abilities.push_back(hal::PowerBatteryIface::Ability::ChargeConfig);
    }

    return info;
}
} // namespace

TestPowerBatteryIface::TestPowerBatteryIface(bool charger_control_supported)
    : hal::PowerBatteryIface(make_battery_info(charger_control_supported))
    , charger_control_supported_(charger_control_supported)
{
    state_ = {
        .is_present = true,
        .power_source = hal::PowerBatteryIface::PowerSource::External,
        .charge_state = hal::PowerBatteryIface::ChargeState::ConstantCurrent,
        .level_source = hal::PowerBatteryIface::LevelSource::FuelGauge,
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

bool TestPowerBatteryIface::get_state(hal::PowerBatteryIface::State &state)
{
    state = state_;

    return true;
}

bool TestPowerBatteryIface::get_charge_config(hal::PowerBatteryIface::ChargeConfig &config)
{
    if (!charger_control_supported_) {
        return false;
    }

    config = charge_config_;

    return true;
}

bool TestPowerBatteryIface::set_charge_config(const hal::PowerBatteryIface::ChargeConfig &config)
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

namespace {

std::shared_ptr<TestPowerBatteryIface> get_battery_iface_or_null()
{
    auto [name, iface] = hal::get_first_interface<hal::PowerBatteryIface>();
    TEST_ASSERT_FALSE(name.empty());
    TEST_ASSERT_NOT_NULL(iface.get());

    auto test_iface = std::dynamic_pointer_cast<TestPowerBatteryIface>(iface);
    TEST_ASSERT_NOT_NULL(test_iface.get());

    return test_iface;
}

} // namespace

TEST_CASE("PowerBatteryIface: get_device finds plugin after registration", "[hal][device]")
{
    auto device = hal::get_device_by_device_name(TestBatteryDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
}

TEST_CASE("PowerBatteryIface: init_device registers interfaces", "[hal][device]")
{
    TestBatteryDevice::set_charger_control_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestBatteryDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::PowerBatteryIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    hal::deinit_device(TestBatteryDevice::NAME);
}

TEST_CASE("PowerBatteryIface: deinit_device releases interfaces", "[hal][device]")
{
    TestBatteryDevice::set_charger_control_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestBatteryDevice::NAME));

    hal::deinit_device(TestBatteryDevice::NAME);

    auto [name, iface] = hal::get_first_interface<hal::PowerBatteryIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("PowerBatteryIface: get_device returns correct device type", "[hal][device]")
{
    TestBatteryDevice::set_charger_control_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestBatteryDevice::NAME));

    auto device = hal::get_device_by_device_name(TestBatteryDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
    TEST_ASSERT_EQUAL_STRING(TestBatteryDevice::NAME, std::string(device->get_name()).c_str());

    hal::deinit_device(TestBatteryDevice::NAME);
}

TEST_CASE("PowerBatteryIface: get_first_interface returns PowerBatteryIface", "[hal][interface]")
{
    TestBatteryDevice::set_charger_control_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestBatteryDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::PowerBatteryIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());
    TEST_ASSERT_EQUAL_STRING(TestPowerBatteryIface::NAME, name.c_str());

    hal::deinit_device(TestBatteryDevice::NAME);
}

TEST_CASE("PowerBatteryIface: get_first_interface returns empty pair when no devices initialized", "[hal][interface]")
{
    auto [name, iface] = hal::get_first_interface<hal::PowerBatteryIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("PowerBatteryIface: get_interfaces returns all instances", "[hal][interface]")
{
    TestBatteryDevice::set_charger_control_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestBatteryDevice::NAME));

    auto interfaces = hal::get_interfaces<hal::PowerBatteryIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, interfaces.size());

    for (const auto &[iface_name, iface] : interfaces) {
        TEST_ASSERT_NOT_NULL(iface.get());
        BROOKESIA_LOGI("Found interface: %1%", iface_name);
    }

    hal::deinit_device(TestBatteryDevice::NAME);
}

TEST_CASE("PowerBatteryIface: Device::get_interface returns interface from device instance", "[hal][interface]")
{
    TestBatteryDevice::set_charger_control_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestBatteryDevice::NAME));

    auto device = hal::get_device_by_device_name(TestBatteryDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::PowerBatteryIface>(TestPowerBatteryIface::NAME);
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::deinit_device(TestBatteryDevice::NAME);
}

TEST_CASE("PowerBatteryIface: Device::get_interface returns nullptr for missing interface", "[hal][interface]")
{
    TestBatteryDevice::set_charger_control_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestBatteryDevice::NAME));

    auto device = hal::get_device_by_device_name(TestBatteryDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::PowerBatteryIface>("missing:Battery");
    TEST_ASSERT_NULL(iface.get());

    hal::deinit_device(TestBatteryDevice::NAME);
}

TEST_CASE("PowerBatteryIface: info abilities are queryable", "[hal][interface]")
{
    TestBatteryDevice::set_charger_control_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestBatteryDevice::NAME));

    auto test_iface = get_battery_iface_or_null();
    const auto &info = test_iface->get_info();
    TEST_ASSERT_EQUAL_STRING("Test battery", info.name.c_str());
    TEST_ASSERT_EQUAL_STRING("Li-ion", info.chemistry.c_str());
    TEST_ASSERT_TRUE(info.has_ability(hal::PowerBatteryIface::Ability::Voltage));
    TEST_ASSERT_TRUE(info.has_ability(hal::PowerBatteryIface::Ability::ChargeConfig));
    TEST_ASSERT_FALSE(info.has_ability(hal::PowerBatteryIface::Ability::SystemVoltage));

    hal::deinit_device(TestBatteryDevice::NAME);
}

TEST_CASE("PowerBatteryIface: state optional fields distinguish available values", "[hal][interface]")
{
    TestBatteryDevice::set_charger_control_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestBatteryDevice::NAME));

    auto test_iface = get_battery_iface_or_null();
    hal::PowerBatteryIface::State state;
    TEST_ASSERT_TRUE(test_iface->get_state(state));
    TEST_ASSERT_TRUE(state.is_present);
    TEST_ASSERT_EQUAL(hal::PowerBatteryIface::PowerSource::External, state.power_source);
    TEST_ASSERT_EQUAL(hal::PowerBatteryIface::ChargeState::ConstantCurrent, state.charge_state);
    TEST_ASSERT_EQUAL(hal::PowerBatteryIface::LevelSource::FuelGauge, state.level_source);
    TEST_ASSERT_TRUE(state.voltage_mv.has_value());
    TEST_ASSERT_EQUAL_UINT32(3850, state.voltage_mv.value());
    TEST_ASSERT_TRUE(state.percentage.has_value());
    TEST_ASSERT_EQUAL_UINT8(67, state.percentage.value());
    TEST_ASSERT_TRUE(state.vbus_voltage_mv.has_value());
    TEST_ASSERT_EQUAL_UINT32(5000, state.vbus_voltage_mv.value());
    TEST_ASSERT_FALSE(state.system_voltage_mv.has_value());
    TEST_ASSERT_FALSE(state.is_low);
    TEST_ASSERT_FALSE(state.is_critical);

    hal::deinit_device(TestBatteryDevice::NAME);
}

TEST_CASE("PowerBatteryIface: charger config supported path", "[hal][interface]")
{
    TestBatteryDevice::set_charger_control_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestBatteryDevice::NAME));

    auto test_iface = get_battery_iface_or_null();
    hal::PowerBatteryIface::ChargeConfig config;
    TEST_ASSERT_TRUE(test_iface->get_charge_config(config));
    TEST_ASSERT_TRUE(config.enabled);
    TEST_ASSERT_EQUAL_UINT32(4100, config.target_voltage_mv);
    TEST_ASSERT_EQUAL_UINT32(400, config.charge_current_ma);
    TEST_ASSERT_EQUAL_UINT32(50, config.precharge_current_ma);
    TEST_ASSERT_EQUAL_UINT32(25, config.termination_current_ma);

    config = {
        .enabled = true,
        .target_voltage_mv = 4200,
        .charge_current_ma = 500,
        .precharge_current_ma = 60,
        .termination_current_ma = 30,
    };
    TEST_ASSERT_TRUE(test_iface->set_charge_config(config));
    TEST_ASSERT_TRUE(test_iface->set_charging_enabled(false));

    hal::PowerBatteryIface::ChargeConfig updated;
    TEST_ASSERT_TRUE(test_iface->get_charge_config(updated));
    TEST_ASSERT_FALSE(updated.enabled);
    TEST_ASSERT_EQUAL_UINT32(4200, updated.target_voltage_mv);
    TEST_ASSERT_EQUAL_UINT32(500, updated.charge_current_ma);
    TEST_ASSERT_EQUAL_UINT32(60, updated.precharge_current_ma);
    TEST_ASSERT_EQUAL_UINT32(30, updated.termination_current_ma);

    hal::deinit_device(TestBatteryDevice::NAME);
}

TEST_CASE("PowerBatteryIface: charger config unsupported path", "[hal][interface]")
{
    TestBatteryDevice::set_charger_control_supported(false);
    TEST_ASSERT_TRUE(hal::init_device(TestBatteryDevice::NAME));

    auto test_iface = get_battery_iface_or_null();
    const auto &info = test_iface->get_info();
    TEST_ASSERT_FALSE(info.has_ability(hal::PowerBatteryIface::Ability::ChargerControl));
    TEST_ASSERT_FALSE(info.has_ability(hal::PowerBatteryIface::Ability::ChargeConfig));

    hal::PowerBatteryIface::ChargeConfig config;
    TEST_ASSERT_FALSE(test_iface->get_charge_config(config));
    TEST_ASSERT_FALSE(test_iface->set_charge_config(config));
    TEST_ASSERT_FALSE(test_iface->set_charging_enabled(false));

    hal::deinit_device(TestBatteryDevice::NAME);
}

TEST_CASE("PowerBatteryIface: repeated init/deinit cycles work correctly", "[hal][device]")
{
    TestBatteryDevice::set_charger_control_supported(true);
    for (int i = 0; i < 3; i++) {
        BROOKESIA_LOGI("Cycle %1%", i + 1);

        TEST_ASSERT_TRUE(hal::init_device(TestBatteryDevice::NAME));

        auto test_iface = get_battery_iface_or_null();
        hal::PowerBatteryIface::State state;
        TEST_ASSERT_TRUE(test_iface->get_state(state));
        TEST_ASSERT_TRUE(state.voltage_mv.has_value());

        hal::deinit_device(TestBatteryDevice::NAME);

        auto [name, iface] = hal::get_first_interface<hal::PowerBatteryIface>();
        TEST_ASSERT_NULL(iface.get());
        TEST_ASSERT_TRUE(name.empty());
    }
}
