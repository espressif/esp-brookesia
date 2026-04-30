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

TEST_CASE("ServiceDevice board: audio control smoke test", "[service][device][board][audio]")
{
    TEST_ASSERT_TRUE_MESSAGE(test_apps::service_device::board::startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        test_apps::service_device::board::shutdown();
    });

    auto capabilities = test_apps::service_device::board::get_capabilities();
    if (capabilities.find(std::string(hal::AudioCodecPlayerIface::NAME)) == capabilities.end()) {
        TEST_IGNORE_MESSAGE("Audio player interface is not available on this board");
    }

    auto current_result = DeviceHelper::call_function_sync<double>(DeviceHelper::FunctionId::GetAudioPlayerVolume);
    TEST_ASSERT_TRUE(current_result.has_value());
    const int current_volume = static_cast<int>(current_result.value());
    const int next_volume = (current_volume >= 100) ? 99 : (current_volume + 1);

    test_apps::service_device::board::EventState volume_event;
    test_apps::service_device::board::EventState mute_event;
    auto volume_connection = DeviceHelper::subscribe_event(
                                 DeviceHelper::EventId::AudioPlayerVolumeChanged,
    [&volume_event](const std::string &, double volume) {
        std::lock_guard<std::mutex> lock(volume_event.mutex);
        volume_event.number = volume;
        volume_event.received = true;
        volume_event.cv.notify_all();
    });
    auto mute_connection = DeviceHelper::subscribe_event(
                               DeviceHelper::EventId::AudioPlayerMuteChanged,
    [&mute_event](const std::string &, bool is_muted) {
        std::lock_guard<std::mutex> lock(mute_event.mutex);
        mute_event.boolean = is_muted;
        mute_event.received = true;
        mute_event.cv.notify_all();
    });
    TEST_ASSERT_TRUE(volume_connection.connected());
    TEST_ASSERT_TRUE(mute_connection.connected());

    auto set_volume_result = DeviceHelper::call_function_sync(
                                 DeviceHelper::FunctionId::SetAudioPlayerVolume,
                                 static_cast<double>(next_volume)
                             );
    TEST_ASSERT_TRUE(set_volume_result.has_value());
    TEST_ASSERT_TRUE(
        test_apps::service_device::board::wait_for_number_event(volume_event, static_cast<double>(next_volume))
    );

    auto get_mute_result = DeviceHelper::call_function_sync<bool>(DeviceHelper::FunctionId::GetAudioPlayerMute);
    TEST_ASSERT_TRUE(get_mute_result.has_value());
    bool next_mute = !get_mute_result.value();

    auto set_mute_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::SetAudioPlayerMute, next_mute);
    TEST_ASSERT_TRUE(set_mute_result.has_value());
    TEST_ASSERT_TRUE(test_apps::service_device::board::wait_for_boolean_event(mute_event, next_mute));

    auto unset_mute_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::SetAudioPlayerMute, !next_mute);
    TEST_ASSERT_TRUE(unset_mute_result.has_value());
    TEST_ASSERT_TRUE(test_apps::service_device::board::wait_for_boolean_event(mute_event, !next_mute));
}
