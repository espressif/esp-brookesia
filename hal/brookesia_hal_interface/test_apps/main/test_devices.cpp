/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <array>
#include "unity.h"
#include "common.hpp"
#include "test_audio_player.hpp"
#include "test_audio_recorder.hpp"
#include "test_display_panel.hpp"
#include "test_display_touch.hpp"
#include "test_display_backlight.hpp"
#include "test_storage_fs.hpp"

using namespace esp_brookesia;

namespace {

constexpr std::array<const char *, 6> EXPECTED_DEVICE_NAMES = {
    TestAudioPlayerDevice::NAME,
    TestAudioRecorderDevice::NAME,
    TestDisplayPanelDevice::NAME,
    TestDisplayTouchDevice::NAME,
    TestDisplayBacklightDevice::NAME,
    TestStorageFsDevice::NAME,
};

constexpr std::array<const char *, 6> EXPECTED_INTERFACE_NAMES = {
    TestAudioCodecPlayerIface::NAME,
    TestAudioCodecRecorderIface::NAME,
    TestDisplayPanelIface::NAME,
    TestDisplayTouchIface::NAME,
    TestDisplayBacklightIface::NAME,
    TestStorageFsIface::NAME,
};

} // namespace

TEST_CASE("Device: init_all_devices initializes all registered test devices and interfaces", "[hal][device]")
{
    hal::deinit_all_devices();

    TEST_ASSERT_TRUE(hal::DeviceRegistry::has_plugin(TestAudioPlayerDevice::NAME));
    TEST_ASSERT_TRUE(hal::DeviceRegistry::has_plugin(TestAudioRecorderDevice::NAME));
    TEST_ASSERT_TRUE(hal::DeviceRegistry::has_plugin(TestDisplayPanelDevice::NAME));
    TEST_ASSERT_TRUE(hal::DeviceRegistry::has_plugin(TestDisplayTouchDevice::NAME));
    TEST_ASSERT_TRUE(hal::DeviceRegistry::has_plugin(TestDisplayBacklightDevice::NAME));
    TEST_ASSERT_TRUE(hal::DeviceRegistry::has_plugin(TestStorageFsDevice::NAME));

    const auto devices_before_init = hal::get_all_devices();
    TEST_ASSERT_GREATER_OR_EQUAL(EXPECTED_DEVICE_NAMES.size(), devices_before_init.size());
    for (const auto *name : EXPECTED_DEVICE_NAMES) {
        TEST_ASSERT_TRUE_MESSAGE(devices_before_init.find(name) != devices_before_init.end(), name);
        TEST_ASSERT_NOT_NULL(devices_before_init.at(name).get());
        TEST_ASSERT_EQUAL_STRING(name, devices_before_init.at(name)->get_name().c_str());
    }

    hal::init_all_devices();

    TEST_ASSERT_TRUE(hal::InterfaceRegistry::has_plugin(TestAudioCodecPlayerIface::NAME));
    TEST_ASSERT_TRUE(hal::InterfaceRegistry::has_plugin(TestAudioCodecRecorderIface::NAME));
    TEST_ASSERT_TRUE(hal::InterfaceRegistry::has_plugin(TestDisplayPanelIface::NAME));
    TEST_ASSERT_TRUE(hal::InterfaceRegistry::has_plugin(TestDisplayTouchIface::NAME));
    TEST_ASSERT_TRUE(hal::InterfaceRegistry::has_plugin(TestDisplayBacklightIface::NAME));
    TEST_ASSERT_TRUE(hal::InterfaceRegistry::has_plugin(TestStorageFsIface::NAME));

    const auto interfaces = hal::get_all_interfaces();
    TEST_ASSERT_GREATER_OR_EQUAL(EXPECTED_INTERFACE_NAMES.size(), interfaces.size());
    for (const auto *name : EXPECTED_INTERFACE_NAMES) {
        TEST_ASSERT_TRUE_MESSAGE(interfaces.find(name) != interfaces.end(), name);
        TEST_ASSERT_NOT_NULL(interfaces.at(name).get());
    }

    hal::deinit_all_devices();
}

TEST_CASE("Device: get_device and get_interface work after init_all_devices", "[hal][device][interface]")
{
    hal::deinit_all_devices();
    hal::init_all_devices();

    auto player_device = hal::get_device_by_device_name(TestAudioPlayerDevice::NAME);
    auto recorder_device = hal::get_device_by_device_name(TestAudioRecorderDevice::NAME);
    auto panel_device = hal::get_device_by_device_name(TestDisplayPanelDevice::NAME);
    auto touch_device = hal::get_device_by_device_name(TestDisplayTouchDevice::NAME);
    auto backlight_device = hal::get_device_by_device_name(TestDisplayBacklightDevice::NAME);
    auto storage_device = hal::get_device_by_device_name(TestStorageFsDevice::NAME);
    const auto devices = hal::get_all_devices();

    TEST_ASSERT_NOT_NULL(player_device.get());
    TEST_ASSERT_NOT_NULL(recorder_device.get());
    TEST_ASSERT_NOT_NULL(panel_device.get());
    TEST_ASSERT_NOT_NULL(touch_device.get());
    TEST_ASSERT_NOT_NULL(backlight_device.get());
    TEST_ASSERT_NOT_NULL(storage_device.get());
    TEST_ASSERT_EQUAL_PTR(player_device.get(), devices.at(TestAudioPlayerDevice::NAME).get());
    TEST_ASSERT_EQUAL_PTR(storage_device.get(), devices.at(TestStorageFsDevice::NAME).get());

    auto player = player_device->get_interface<hal::AudioCodecPlayerIface>(TestAudioCodecPlayerIface::NAME);
    auto recorder = recorder_device->get_interface<hal::AudioCodecRecorderIface>(TestAudioCodecRecorderIface::NAME);
    auto panel = panel_device->get_interface<hal::DisplayPanelIface>(TestDisplayPanelIface::NAME);
    auto touch = touch_device->get_interface<hal::DisplayTouchIface>(TestDisplayTouchIface::NAME);
    auto backlight = backlight_device->get_interface<hal::DisplayBacklightIface>(TestDisplayBacklightIface::NAME);
    auto storage = storage_device->get_interface<hal::StorageFsIface>(TestStorageFsIface::NAME);
    const auto interfaces = hal::get_all_interfaces();

    TEST_ASSERT_NOT_NULL(player.get());
    TEST_ASSERT_NOT_NULL(recorder.get());
    TEST_ASSERT_NOT_NULL(panel.get());
    TEST_ASSERT_NOT_NULL(touch.get());
    TEST_ASSERT_NOT_NULL(backlight.get());
    TEST_ASSERT_NOT_NULL(storage.get());
    TEST_ASSERT_EQUAL_PTR(player.get(), std::dynamic_pointer_cast<hal::AudioCodecPlayerIface>(
                              interfaces.at(TestAudioCodecPlayerIface::NAME)).get());
    TEST_ASSERT_EQUAL_PTR(storage.get(), std::dynamic_pointer_cast<hal::StorageFsIface>(
                              interfaces.at(TestStorageFsIface::NAME)).get());

    TEST_ASSERT_NULL(player_device->get_interface<hal::StorageFsIface>("missing:StorageFs").get());
    TEST_ASSERT_NULL(storage_device->get_interface<hal::AudioCodecPlayerIface>("missing:AudioPlayer").get());

    hal::deinit_all_devices();
}

TEST_CASE("Device: deinit_all_devices clears registered interfaces", "[hal][device]")
{
    hal::deinit_all_devices();
    hal::init_all_devices();
    hal::deinit_all_devices();

    const auto interfaces = hal::get_all_interfaces();
    TEST_ASSERT_TRUE(interfaces.empty());
}

TEST_CASE("Device: init_device handles unknown device name", "[hal][device]")
{
    hal::deinit_all_devices();

    TEST_ASSERT_FALSE(hal::init_device("UnknownDevice"));

    // Should be a no-op and must not crash.
    hal::deinit_device("UnknownDevice");
}
