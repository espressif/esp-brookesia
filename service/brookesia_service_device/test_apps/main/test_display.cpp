/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <mutex>
#include <string>
#include "unity.h"
#include "brookesia/hal_interface.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_helper/device.hpp"
#include "test_support.hpp"

using namespace esp_brookesia;
using DeviceHelper = service::helper::Device;

TEST_CASE("ServiceDevice board: display control smoke test", "[service][device][board][display]")
{
    TEST_ASSERT_TRUE_MESSAGE(test_apps::service_device::board::startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        test_apps::service_device::board::shutdown();
    });

    auto capabilities = test_apps::service_device::board::get_capabilities();
    if (capabilities.find(std::string(hal::DisplayBacklightIface::NAME)) == capabilities.end()) {
        TEST_IGNORE_MESSAGE("Display backlight interface is not available on this board");
    }

    auto current_result = DeviceHelper::call_function_sync<double>(DeviceHelper::FunctionId::GetDisplayBacklightBrightness);
    TEST_ASSERT_TRUE(current_result.has_value());
    const int current_brightness = static_cast<int>(current_result.value());
    const int next_brightness = (current_brightness >= 100) ? 99 : (current_brightness + 1);

    test_apps::service_device::board::EventState brightness_event;
    test_apps::service_device::board::EventState on_off_event;
    auto brightness_connection = DeviceHelper::subscribe_event(
                                     DeviceHelper::EventId::DisplayBacklightBrightnessChanged,
    [&brightness_event](const std::string &, double brightness) {
        std::lock_guard<std::mutex> lock(brightness_event.mutex);
        brightness_event.number = brightness;
        brightness_event.received = true;
        brightness_event.cv.notify_all();
    });
    auto on_off_connection = DeviceHelper::subscribe_event(
                                 DeviceHelper::EventId::DisplayBacklightOnOffChanged,
    [&on_off_event](const std::string &, bool is_on) {
        std::lock_guard<std::mutex> lock(on_off_event.mutex);
        on_off_event.boolean = is_on;
        on_off_event.received = true;
        on_off_event.cv.notify_all();
    });
    TEST_ASSERT_TRUE(brightness_connection.connected());
    TEST_ASSERT_TRUE(on_off_connection.connected());

    auto set_brightness_result = DeviceHelper::call_function_sync(
                                     DeviceHelper::FunctionId::SetDisplayBacklightBrightness,
                                     static_cast<double>(next_brightness)
                                 );
    TEST_ASSERT_TRUE(set_brightness_result.has_value());
    TEST_ASSERT_TRUE(
        test_apps::service_device::board::wait_for_number_event(brightness_event, static_cast<double>(next_brightness))
    );

    auto get_on_off_result = DeviceHelper::call_function_sync<bool>(DeviceHelper::FunctionId::GetDisplayBacklightOnOff);
    TEST_ASSERT_TRUE(get_on_off_result.has_value());
    bool next_on_off = !get_on_off_result.value();

    auto set_off_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::SetDisplayBacklightOnOff, next_on_off);
    TEST_ASSERT_TRUE(set_off_result.has_value());
    TEST_ASSERT_TRUE(test_apps::service_device::board::wait_for_boolean_event(on_off_event, next_on_off));

    auto set_on_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::SetDisplayBacklightOnOff, !next_on_off);
    TEST_ASSERT_TRUE(set_on_result.has_value());
    TEST_ASSERT_TRUE(test_apps::service_device::board::wait_for_boolean_event(on_off_event, !next_on_off));
}
