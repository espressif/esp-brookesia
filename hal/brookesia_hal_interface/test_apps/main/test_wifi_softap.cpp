/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <memory>

#include "unity.h"
#include "common.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "test_wifi_common.hpp"

using namespace esp_brookesia;

TEST_CASE("wifi::SoftApIface: acquire softap interface", "[hal][device]")
{

    auto iface_handle = hal::acquire_first_interface<hal::wifi::SoftApIface>();
    auto name = std::string(iface_handle.instance_name());
    auto iface = iface_handle.get();
    TEST_ASSERT_EQUAL_STRING(TestWifiBackend::SOFTAP_NAME, name.c_str());
    TEST_ASSERT_NOT_NULL(iface.get());
}

TEST_CASE("wifi::SoftApIface: params actions and callbacks", "[hal][interface]")
{

    auto basic_iface_handle = hal::acquire_first_interface<hal::wifi::BasicIface>();
    auto basic_name = std::string(basic_iface_handle.instance_name());
    auto basic_iface = basic_iface_handle.get();
    auto softap_iface_handle = hal::acquire_first_interface<hal::wifi::SoftApIface>();
    auto softap_name = std::string(softap_iface_handle.instance_name());
    auto softap_iface = softap_iface_handle.get();
    TEST_ASSERT_NOT_NULL(basic_iface.get());
    TEST_ASSERT_NOT_NULL(softap_iface.get());
    TEST_ASSERT_FALSE(basic_name.empty());
    TEST_ASSERT_FALSE(softap_name.empty());

    auto scheduler = std::make_shared<lib_utils::TaskScheduler>();
    TEST_ASSERT_TRUE(basic_iface->configure(
    hal::wifi::BasicIface::RuntimeContext{
        .task_scheduler = scheduler,
        .task_group = "wifi-test",
    },
    {}
                     ));
    TEST_ASSERT_TRUE(basic_iface->init());
    TEST_ASSERT_TRUE(basic_iface->start());

    hal::wifi::SoftApEvent last_event = hal::wifi::SoftApEvent::Max;
    hal::wifi::SoftApParams updated_params{};
    hal::wifi::StationAction requested_action = hal::wifi::StationAction::Max;

    TEST_ASSERT_TRUE(softap_iface->configure({
        .on_event = [&](hal::wifi::SoftApEvent event)
        {
            last_event = event;
        },
        .on_params_updated = [&](const hal::wifi::SoftApParams & params)
        {
            updated_params = params;
        },
        .on_station_action_requested = [&](hal::wifi::StationAction action)
        {
            requested_action = action;
            return true;
        },
    }));

    hal::wifi::SoftApParams softap_params{
        .ssid = "SoftAP",
        .password = "12345678",
        .max_connection = 3,
        .channel = 6,
    };
    TEST_ASSERT_TRUE(softap_iface->set_params(softap_params));
    TEST_ASSERT_EQUAL_STRING("SoftAP", softap_iface->get_params().ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("SoftAP", updated_params.ssid.c_str());

    TEST_ASSERT_TRUE(softap_iface->start());
    TEST_ASSERT_EQUAL(hal::wifi::SoftApEvent::Started, last_event);

    softap_iface->stop();
    TEST_ASSERT_EQUAL(hal::wifi::SoftApEvent::Stopped, last_event);

    TEST_ASSERT_TRUE(softap_iface->start_provision());
    TEST_ASSERT_EQUAL(hal::wifi::SoftApEvent::Started, last_event);
    TEST_ASSERT_EQUAL(hal::wifi::StationAction::Disconnect, requested_action);

    TEST_ASSERT_TRUE(softap_iface->stop_provision());
    TEST_ASSERT_EQUAL(hal::wifi::SoftApEvent::Stopped, last_event);

    softap_iface->clear_callbacks();
    basic_iface->deinit();
}
