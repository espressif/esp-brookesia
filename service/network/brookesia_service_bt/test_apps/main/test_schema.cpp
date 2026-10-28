/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#ifndef ESP_PLATFORM

#define BOOST_TEST_MODULE brookesia_service_bt
#include <boost/test/unit_test.hpp>

#include "brookesia/service_helper/network/bt.hpp"
#include "brookesia/service_helper/framework/manager.hpp"
#include "brookesia/service_manager.hpp"

namespace {

using BtHelper = esp_brookesia::service::helper::Bt;
using ManagerHelper = esp_brookesia::service::helper::Manager;
using esp_brookesia::service::ServiceManager;

BtHelper::State get_state()
{
    auto result = BtHelper::call_function_sync<boost::json::object>(BtHelper::FunctionId::GetState);
    BOOST_REQUIRE(result.has_value());

    BtHelper::State state;
    BOOST_REQUIRE(BROOKESIA_DESCRIBE_FROM_JSON(*result, state));
    return state;
}

class ManagerFixture {
public:
    ManagerFixture()
    {
        BOOST_REQUIRE(manager_.init());
        BOOST_REQUIRE(manager_.start());
    }

    ~ManagerFixture()
    {
        manager_.stop();
        manager_.deinit();
    }

    ServiceManager &manager()
    {
        return manager_;
    }

private:
    ServiceManager &manager_ = ServiceManager::get_instance();
};

} // namespace

BOOST_AUTO_TEST_CASE(real_bt_service_start_smoke)
{
    ManagerFixture fixture;

    BOOST_TEST(BtHelper::get_name() == "Bt");
    BOOST_TEST(esp_brookesia::service::ServiceRegistry::has_plugin("Bt"));

    auto binding = fixture.manager().bind(BtHelper::get_name().data());
    BOOST_REQUIRE(binding.is_valid());

    auto info = ManagerHelper::get_service_info(BtHelper::get_name().data(), 1000);
    BOOST_REQUIRE(info.has_value());
    BOOST_TEST(info->name == "Bt");
    BOOST_TEST(info->version == "0.8.0");
    auto schema = ManagerHelper::get_service_schema(BtHelper::get_name().data(), 1000);
    BOOST_REQUIRE(schema.has_value());
    BOOST_TEST(schema->function_names.size() == 17);
    BOOST_TEST(schema->event_names.size() == 8);

    auto capabilities_result = BtHelper::call_function_sync<boost::json::object>(
                                   BtHelper::FunctionId::GetCapabilities
                               );
    BOOST_REQUIRE(capabilities_result.has_value());
    BtHelper::Capabilities capabilities;
    BOOST_REQUIRE(BROOKESIA_DESCRIBE_FROM_JSON(*capabilities_result, capabilities));
#if !defined(ESP_PLATFORM)
    BOOST_REQUIRE(capabilities.classic_supported);
#endif

    const BtHelper::DeviceConfig device_config{
        .device_name = "ESP-Brookesia-Bt-Test",
        .discoverable = true,
        .connectable = true,
    };
    auto set_config_result = BtHelper::call_function_sync(
                                 BtHelper::FunctionId::SetDeviceConfig,
                                 BROOKESIA_DESCRIBE_TO_JSON(device_config).as_object()
                             );
    BOOST_REQUIRE(set_config_result.has_value());

    auto start_result = BtHelper::call_function_sync(BtHelper::FunctionId::A2dpSinkStart);
    auto state = get_state();
    if (capabilities.classic_supported) {
        BOOST_REQUIRE(start_result.has_value());
        BOOST_TEST(
            static_cast<int>(state.host_state) == static_cast<int>(BtHelper::HostState::Started)
        );
        BOOST_TEST(state.is_started);
    } else {
        BOOST_TEST(!start_result.has_value());
        BOOST_TEST(!start_result.error().empty());
        BOOST_TEST(static_cast<int>(state.host_state) == static_cast<int>(BtHelper::HostState::Error));
        BOOST_TEST(!state.is_started);
    }

    auto connections_result = BtHelper::call_function_sync<boost::json::array>(
                                  BtHelper::FunctionId::GetConnections
                              );
    BOOST_REQUIRE(connections_result.has_value());
    BOOST_TEST(connections_result->empty());

    auto stop_result = BtHelper::call_function_sync(BtHelper::FunctionId::A2dpSinkStop);
    BOOST_REQUIRE(stop_result.has_value());
    state = get_state();
    BOOST_TEST(static_cast<int>(state.host_state) == static_cast<int>(BtHelper::HostState::Idle));
    BOOST_TEST(!state.is_started);
}

#endif // ESP_PLATFORM
