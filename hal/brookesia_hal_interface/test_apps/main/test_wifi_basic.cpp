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


TEST_CASE("wifi::BasicIface: acquire basic interface", "[hal][interface]")
{
    auto handle = hal::acquire_first_interface<hal::wifi::BasicIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_EQUAL_STRING(TestWifiBackend::BASIC_NAME, std::string(handle.instance_name()).c_str());
}

TEST_CASE("wifi::BasicIface: configure lifecycle and callbacks", "[hal][interface]")
{
    auto handle = hal::acquire_first_interface<hal::wifi::BasicIface>();
    auto iface = handle.get();
    TEST_ASSERT_NOT_NULL(iface.get());

    TEST_ASSERT_FALSE(iface->configure({}, {}));

    auto scheduler = std::make_shared<lib_utils::TaskScheduler>();
    hal::wifi::BasicAction last_action = hal::wifi::BasicAction::Max;
    hal::wifi::BasicEvent last_event = hal::wifi::BasicEvent::Max;
    bool last_unexpected = true;
    int error_count = 0;

    TEST_ASSERT_TRUE(iface->configure(
                         hal::wifi::BasicIface::RuntimeContext{.task_scheduler = scheduler, .task_group = "wifi-test"},
    hal::wifi::BasicIface::Callbacks{
        .on_action = [&](hal::wifi::BasicAction action)
        {
            last_action = action;
        },
        .on_event = [&](hal::wifi::BasicEvent event, bool is_unexpected)
        {
            last_event = event;
            last_unexpected = is_unexpected;
        },
        .on_error = [&]()
        {
            error_count++;
        },
    }
                     ));

    TEST_ASSERT_TRUE(iface->init());
    TEST_ASSERT_TRUE(iface->start());
    TEST_ASSERT_TRUE(iface->do_action(hal::wifi::BasicAction::Init));
    TEST_ASSERT_EQUAL(hal::wifi::BasicAction::Init, last_action);
    TEST_ASSERT_EQUAL(hal::wifi::BasicEvent::Inited, last_event);
    TEST_ASSERT_FALSE(last_unexpected);
    TEST_ASSERT_TRUE(iface->is_event_ready(hal::wifi::BasicEvent::Inited));

    TEST_ASSERT_TRUE(iface->do_action(hal::wifi::BasicAction::Start));
    TEST_ASSERT_EQUAL(hal::wifi::BasicEvent::Started, last_event);
    TEST_ASSERT_TRUE(iface->do_action(hal::wifi::BasicAction::Stop));
    TEST_ASSERT_EQUAL(hal::wifi::BasicEvent::Stopped, last_event);
    TEST_ASSERT_TRUE(iface->do_action(hal::wifi::BasicAction::Deinit));
    TEST_ASSERT_EQUAL(hal::wifi::BasicEvent::Deinited, last_event);
    TEST_ASSERT_EQUAL(0, error_count);

    iface->clear_callbacks();
    iface->deinit();
}
