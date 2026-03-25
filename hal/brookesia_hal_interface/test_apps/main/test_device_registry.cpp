/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <memory>
#include <vector>

#include "unity.h"

#include "test_fixtures.hpp"

using namespace esp_brookesia::hal;
using namespace hal_test;

TEST_CASE("HAL interface registry initializes registered devices", "[hal][device][registry]")
{
    Device::Registry::register_plugin<MockStatusLedDevice>("status_led", []() {
        return std::static_pointer_cast<Device>(std::make_shared<MockStatusLedDevice>("status_led"));
    });

    TEST_ASSERT_TRUE(Device::init_device_from_registry());

    auto status = std::static_pointer_cast<MockStatusLedDevice>(get_device("status_led"));

    TEST_ASSERT_NOT_NULL(status.get());
    TEST_ASSERT_EQUAL_UINT32(1, status->probe_count);
    TEST_ASSERT_EQUAL_UINT32(1, status->init_count);
    TEST_ASSERT_TRUE(status->check_initialized());

}

TEST_CASE("HAL interface get_interfaces filters by supported interface and preserves registration order", "[hal][device][interfaces]")
{
    Device::Registry::register_plugin<MockStatusLedDevice>("status_led", []() {
        return std::static_pointer_cast<Device>(std::make_shared<MockStatusLedDevice>("status_led"));
    });

    auto status_ifaces = get_interfaces<StatusLedIface>();
    TEST_ASSERT_EQUAL_UINT32(1, status_ifaces.size());
    TEST_ASSERT_EQUAL_STRING("status_led", status_ifaces[0].first.c_str());
    TEST_ASSERT_NOT_NULL(status_ifaces[0].second);
}

TEST_CASE("HAL DeviceImpl query exposes supported interfaces only", "[hal][device][query]")
{
    MockAudioDevice audio_device("audio");

    TEST_ASSERT_NOT_NULL(audio_device.query<AudioPlayerIface>());
    TEST_ASSERT_NOT_NULL(audio_device.query<AudioRecorderIface>());
    TEST_ASSERT_NULL(audio_device.query<DisplayPanelIface>());
    TEST_ASSERT_NULL(audio_device.query<StorageFsIface>());
}
