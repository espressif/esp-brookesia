/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#ifndef ESP_PLATFORM

#define BOOST_TEST_MODULE brookesia_service_bt_speaker
#include <boost/test/unit_test.hpp>

#include "brookesia/service_helper/framework/manager.hpp"
#include "brookesia/service_helper/media/bt_speaker.hpp"
#include "brookesia/service_helper/network/bt.hpp"
#include "brookesia/service_manager.hpp"

namespace {

using BtHelper = esp_brookesia::service::helper::Bt;
using Helper = esp_brookesia::service::helper::BtSpeaker;
using ManagerHelper = esp_brookesia::service::helper::Manager;
using esp_brookesia::service::ServiceManager;

BtHelper::State get_bt_state()
{
    auto result = BtHelper::call_function_sync<boost::json::object>(BtHelper::FunctionId::GetState);
    BOOST_REQUIRE(result.has_value());

    BtHelper::State state;
    BOOST_REQUIRE(BROOKESIA_DESCRIBE_FROM_JSON(*result, state));
    return state;
}

Helper::State get_speaker_state()
{
    auto result = Helper::call_function_sync<boost::json::object>(Helper::FunctionId::GetState);
    BOOST_REQUIRE(result.has_value());

    Helper::State state;
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

BOOST_AUTO_TEST_CASE(real_bt_speaker_service_start_smoke)
{
    ManagerFixture fixture;

    BOOST_TEST(Helper::get_name() == "BtSpeaker");
    BOOST_TEST(esp_brookesia::service::ServiceRegistry::has_plugin(BtHelper::get_name().data()));
    BOOST_TEST(esp_brookesia::service::ServiceRegistry::has_plugin(Helper::get_name().data()));

    auto speaker_binding = fixture.manager().bind(Helper::get_name().data());
    BOOST_REQUIRE(speaker_binding.is_valid());

    auto info = ManagerHelper::get_service_info(Helper::get_name().data(), 1000);
    BOOST_REQUIRE(info.has_value());
    BOOST_TEST(info->name == "BtSpeaker");
    BOOST_TEST(info->version == "0.8.0");
    auto schema = ManagerHelper::get_service_schema(Helper::get_name().data(), 1000);
    BOOST_REQUIRE(schema.has_value());
    BOOST_TEST(schema->function_names.size() == 12);
    BOOST_TEST(schema->event_names.size() == 7);

    auto capabilities_result = BtHelper::call_function_sync<boost::json::object>(
                                   BtHelper::FunctionId::GetCapabilities
                               );
    BOOST_REQUIRE(capabilities_result.has_value());
    BtHelper::Capabilities capabilities;
    BOOST_REQUIRE(BROOKESIA_DESCRIBE_FROM_JSON(*capabilities_result, capabilities));
    BOOST_REQUIRE(capabilities.classic_supported);

    const Helper::Config config{
        .device = {
            .device_name = "ESP-Brookesia-BtSpeaker-Test",
            .discoverable = true,
            .connectable = true,
        },
        .stop_local_playback_on_connect = true,
    };
    auto set_config_result = Helper::call_function_sync(
                                 Helper::FunctionId::SetConfig,
                                 BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                             );
    BOOST_REQUIRE(set_config_result.has_value());

    auto start_result = Helper::call_function_sync(Helper::FunctionId::Start);
    BOOST_REQUIRE(start_result.has_value());
    auto speaker_state = get_speaker_state();
    auto bt_state = get_bt_state();
    BOOST_TEST(
        static_cast<int>(speaker_state.general_state) == static_cast<int>(Helper::GeneralState::Started)
    );
    BOOST_TEST(speaker_state.is_started);
    BOOST_TEST(!speaker_state.is_connected);
    BOOST_TEST(!speaker_state.is_music_active);
    BOOST_TEST(static_cast<int>(bt_state.host_state) == static_cast<int>(BtHelper::HostState::Started));
    BOOST_TEST(bt_state.is_started);

    auto stop_result = Helper::call_function_sync(Helper::FunctionId::Stop);
    BOOST_REQUIRE(stop_result.has_value());
    speaker_state = get_speaker_state();
    bt_state = get_bt_state();
    BOOST_TEST(
        static_cast<int>(speaker_state.general_state) == static_cast<int>(Helper::GeneralState::Ready)
    );
    BOOST_TEST(speaker_state.is_configured);
    BOOST_TEST(!speaker_state.is_started);
    BOOST_TEST(!bt_state.is_started);
    BOOST_TEST(static_cast<int>(bt_state.host_state) == static_cast<int>(BtHelper::HostState::Idle));
}

#endif // ESP_PLATFORM
