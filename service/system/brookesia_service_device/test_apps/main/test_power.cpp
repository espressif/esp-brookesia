/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "brookesia/lib_utils/test_adapter.hpp"
#include "boost/json.hpp"
#if defined(ESP_PLATFORM)
#include "brookesia/hal_adaptor.hpp"
#else
#include "brookesia/hal_linux.hpp"
#endif
#include "brookesia/hal_interface.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_helper/system/device.hpp"
#include "brookesia/service_manager.hpp"
#include "test_support.hpp"

using namespace esp_brookesia;
using DeviceHelper = service::helper::Device;

BROOKESIA_TEST_CASE(test_servicedevice_board_power_battery_query_smoke_test, "ServiceDevice board: power battery query smoke test", "[service][device][board][power][battery]")
{
    TEST_ASSERT_TRUE_MESSAGE(test_apps::service_device::board::startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        test_apps::service_device::board::shutdown();
    });

    auto capabilities = test_apps::service_device::board::get_capabilities();
    if (!test_apps::service_device::board::has_capability(capabilities, hal::power::BatteryIface::NAME)) {
        TEST_IGNORE_MESSAGE("Power battery interface is not available on this board");
    }

    auto battery_info_result = DeviceHelper::call_function_sync<boost::json::object>(
                                   DeviceHelper::FunctionId::GetPowerBatteryInfo
                               );
    TEST_ASSERT_TRUE(battery_info_result.has_value());

    DeviceHelper::PowerBatteryInfo info;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(battery_info_result.value(), info));

    auto battery_state_result = DeviceHelper::call_function_sync<boost::json::object>(
                                    DeviceHelper::FunctionId::GetPowerBatteryState
                                );
    TEST_ASSERT_TRUE(battery_state_result.has_value());

    DeviceHelper::PowerBatteryState state;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(battery_state_result.value(), state));
    if (info.has_ability(hal::power::BatteryIface::Ability::Voltage)) {
        TEST_ASSERT_TRUE(state.voltage_mv.has_value());
    }
    if (info.has_ability(hal::power::BatteryIface::Ability::Percentage)) {
        TEST_ASSERT_TRUE(state.percentage.has_value());
    }
    if (info.has_ability(hal::power::BatteryIface::Ability::VbusVoltage)) {
        TEST_ASSERT_TRUE(state.vbus_voltage_mv.has_value());
    }
    if (info.has_ability(hal::power::BatteryIface::Ability::SystemVoltage)) {
        TEST_ASSERT_TRUE(state.system_voltage_mv.has_value());
    }

    DeviceHelper::PowerBatteryChargeConfig config = {};
    if (info.has_ability(hal::power::BatteryIface::Ability::ChargeConfig)) {
        auto config_result = DeviceHelper::call_function_sync<boost::json::object>(
                                 DeviceHelper::FunctionId::GetPowerBatteryChargeConfig
                             );
        TEST_ASSERT_TRUE(config_result.has_value());
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(config_result.value(), config));

        auto set_config_result = DeviceHelper::call_function_sync(
                                     DeviceHelper::FunctionId::SetPowerBatteryChargeConfig,
                                     BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                                 );
        TEST_ASSERT_TRUE(set_config_result.has_value());
    } else {
        auto set_config_result = DeviceHelper::call_function_sync(
                                     DeviceHelper::FunctionId::SetPowerBatteryChargeConfig,
                                     BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                                 );
        TEST_ASSERT_FALSE(set_config_result.has_value());
    }

    if (info.has_ability(hal::power::BatteryIface::Ability::ChargerControl) &&
            info.has_ability(hal::power::BatteryIface::Ability::ChargeConfig)) {
        auto set_enabled_result = DeviceHelper::call_function_sync(
                                      DeviceHelper::FunctionId::SetPowerBatteryChargingEnabled, config.enabled
                                  );
        TEST_ASSERT_TRUE(set_enabled_result.has_value());
    } else if (!info.has_ability(hal::power::BatteryIface::Ability::ChargerControl)) {
        auto set_enabled_result = DeviceHelper::call_function_sync(
                                      DeviceHelper::FunctionId::SetPowerBatteryChargingEnabled, false
                                  );
        TEST_ASSERT_FALSE(set_enabled_result.has_value());
    }
}

BROOKESIA_TEST_CASE(test_servicedevice_board_power_battery_initial_event_smoke_test, "ServiceDevice board: power battery initial event smoke test", "[service][device][board][power][battery][event]")
{
    auto &service_manager = service::ServiceManager::get_instance();

    lib_utils::FunctionGuard shutdown_guard([&service_manager]() {
        service_manager.deinit();
    });

    if (!hal::has_interface(hal::power::BatteryIface::NAME)) {
        TEST_IGNORE_MESSAGE("Power battery interface is not available on this board");
    }

    TEST_ASSERT_TRUE_MESSAGE(service_manager.start(), "Failed to start service manager");

    DeviceHelper::EventMonitor<DeviceHelper::EventId::PowerBatteryStateChanged> state_monitor;
    DeviceHelper::EventMonitor<DeviceHelper::EventId::PowerBatteryChargeConfigChanged> config_monitor;
    TEST_ASSERT_TRUE(state_monitor.start());
    TEST_ASSERT_TRUE(config_monitor.start());

    auto device_binding = service_manager.bind(DeviceHelper::get_name().data());
    TEST_ASSERT_TRUE(device_binding.is_valid());

    TEST_ASSERT_TRUE(state_monitor.wait_for_any(2500));
    auto state_event = state_monitor.get_last<boost::json::object>();
    TEST_ASSERT_TRUE(state_event.has_value());
    DeviceHelper::PowerBatteryState state;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(std::get<0>(state_event.value()), state));

    auto battery_info_result = DeviceHelper::call_function_sync<boost::json::object>(
                                   DeviceHelper::FunctionId::GetPowerBatteryInfo
                               );
    TEST_ASSERT_TRUE(battery_info_result.has_value());

    DeviceHelper::PowerBatteryInfo info;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(battery_info_result.value(), info));

    if (info.has_ability(hal::power::BatteryIface::Ability::ChargeConfig)) {
        TEST_ASSERT_TRUE(config_monitor.wait_for_any(2500));
        auto config_event = config_monitor.get_last<boost::json::object>();
        TEST_ASSERT_TRUE(config_event.has_value());
        DeviceHelper::PowerBatteryChargeConfig config;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(std::get<0>(config_event.value()), config));
    }

    device_binding.release();
}
