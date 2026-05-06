/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <mutex>
#include "unity.h"
#include "boost/json.hpp"
#include "brookesia/hal_adaptor.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_helper/device.hpp"
#include "brookesia/service_manager.hpp"
#include "test_support.hpp"

using namespace esp_brookesia;
using DeviceHelper = service::helper::Device;

TEST_CASE("ServiceDevice board: power battery query smoke test", "[service][device][board][power][battery]")
{
    TEST_ASSERT_TRUE_MESSAGE(test_apps::service_device::board::startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        test_apps::service_device::board::shutdown();
    });

    auto capabilities = test_apps::service_device::board::get_capabilities();
    if (capabilities.find(std::string(hal::PowerBatteryIface::NAME)) == capabilities.end()) {
        TEST_IGNORE_MESSAGE("Power battery interface is not available on this board");
    }

    auto battery_info_result = DeviceHelper::call_function_sync<boost::json::object>(
                                   DeviceHelper::FunctionId::GetPowerBatteryInfo
                               );
    TEST_ASSERT_TRUE(battery_info_result.has_value());

    DeviceHelper::PowerBatteryInfo info;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(battery_info_result.value(), info));
    TEST_ASSERT_TRUE(info.has_ability(hal::PowerBatteryIface::Ability::Voltage));

    auto battery_state_result = DeviceHelper::call_function_sync<boost::json::object>(
                                    DeviceHelper::FunctionId::GetPowerBatteryState
                                );
    TEST_ASSERT_TRUE(battery_state_result.has_value());

    DeviceHelper::PowerBatteryState state;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(battery_state_result.value(), state));

    DeviceHelper::PowerBatteryChargeConfig config = {};
    if (info.has_ability(hal::PowerBatteryIface::Ability::ChargeConfig)) {
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

    if (info.has_ability(hal::PowerBatteryIface::Ability::ChargerControl) &&
            info.has_ability(hal::PowerBatteryIface::Ability::ChargeConfig)) {
        auto set_enabled_result = DeviceHelper::call_function_sync(
                                      DeviceHelper::FunctionId::SetPowerBatteryChargingEnabled, config.enabled
                                  );
        TEST_ASSERT_TRUE(set_enabled_result.has_value());
    } else if (!info.has_ability(hal::PowerBatteryIface::Ability::ChargerControl)) {
        auto set_enabled_result = DeviceHelper::call_function_sync(
                                      DeviceHelper::FunctionId::SetPowerBatteryChargingEnabled, false
                                  );
        TEST_ASSERT_FALSE(set_enabled_result.has_value());
    }
}

TEST_CASE("ServiceDevice board: power battery initial event smoke test", "[service][device][board][power][battery][event]")
{
    auto &service_manager = service::ServiceManager::get_instance();

    hal::init_all_devices();
    lib_utils::FunctionGuard shutdown_guard([&service_manager]() {
        service_manager.deinit();
        hal::deinit_all_devices();
    });

    auto [battery_name, battery_iface] = hal::get_first_interface<hal::PowerBatteryIface>();
    if (!battery_iface) {
        TEST_IGNORE_MESSAGE("Power battery interface is not available on this board");
    }

    TEST_ASSERT_TRUE_MESSAGE(service_manager.start(), "Failed to start service manager");

    test_apps::service_device::board::EventState state_event;
    auto state_connection = DeviceHelper::subscribe_event(
                                DeviceHelper::EventId::PowerBatteryStateChanged,
    [&state_event](const std::string &, const boost::json::object & state_json) {
        std::lock_guard<std::mutex> lock(state_event.mutex);
        state_event.object = state_json;
        state_event.received = true;
        state_event.cv.notify_all();
    });
    TEST_ASSERT_TRUE(state_connection.connected());

    test_apps::service_device::board::EventState config_event;
    auto config_connection = DeviceHelper::subscribe_event(
                                 DeviceHelper::EventId::PowerBatteryChargeConfigChanged,
    [&config_event](const std::string &, const boost::json::object & config_json) {
        std::lock_guard<std::mutex> lock(config_event.mutex);
        config_event.object = config_json;
        config_event.received = true;
        config_event.cv.notify_all();
    });
    TEST_ASSERT_TRUE(config_connection.connected());

    auto device_binding = service_manager.bind(DeviceHelper::get_name().data());
    TEST_ASSERT_TRUE(device_binding.is_valid());

    TEST_ASSERT_TRUE(test_apps::service_device::board::wait_for_object_event(state_event, 2500));
    DeviceHelper::PowerBatteryState state;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(state_event.object, state));

    if (battery_iface->get_info().has_ability(hal::PowerBatteryIface::Ability::ChargeConfig)) {
        TEST_ASSERT_TRUE(test_apps::service_device::board::wait_for_object_event(config_event, 2500));
        DeviceHelper::PowerBatteryChargeConfig config;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(config_event.object, config));
    }

    device_binding.release();
}
