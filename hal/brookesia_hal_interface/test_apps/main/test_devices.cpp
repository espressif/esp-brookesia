/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.hpp"
#include "test_audio_player.hpp"
#include "test_audio_recorder.hpp"
#include "test_display_panel.hpp"
#include "test_display_touch.hpp"
#include "test_display_backlight.hpp"
#include "test_storage_fs.hpp"

using namespace esp_brookesia;

TEST_CASE("Device: init_all_devices initializes all registered test devices and interfaces", "[hal][device]")
{
    hal::deinit_all_devices();

    TEST_ASSERT_TRUE(hal::DeviceRegistry::has_plugin(TestAudioPlayerDevice::NAME));
    TEST_ASSERT_TRUE(hal::DeviceRegistry::has_plugin(TestAudioRecorderDevice::NAME));
    TEST_ASSERT_TRUE(hal::DeviceRegistry::has_plugin(TestDisplayPanelDevice::NAME));
    TEST_ASSERT_TRUE(hal::DeviceRegistry::has_plugin(TestDisplayTouchDevice::NAME));
    TEST_ASSERT_TRUE(hal::DeviceRegistry::has_plugin(TestDisplayBacklightDevice::NAME));
    TEST_ASSERT_TRUE(hal::DeviceRegistry::has_plugin(TestStorageFsDevice::NAME));

    hal::init_all_devices();

    TEST_ASSERT_TRUE(hal::InterfaceRegistry::has_plugin(TestAudioCodecPlayerIface::NAME));
    TEST_ASSERT_TRUE(hal::InterfaceRegistry::has_plugin(TestAudioCodecRecorderIface::NAME));
    TEST_ASSERT_TRUE(hal::InterfaceRegistry::has_plugin(TestDisplayPanelIface::NAME));
    TEST_ASSERT_TRUE(hal::InterfaceRegistry::has_plugin(TestDisplayTouchIface::NAME));
    TEST_ASSERT_TRUE(hal::InterfaceRegistry::has_plugin(TestDisplayBacklightIface::NAME));
    TEST_ASSERT_TRUE(hal::InterfaceRegistry::has_plugin(TestStorageFsIface::NAME));

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

    TEST_ASSERT_NOT_NULL(player_device.get());
    TEST_ASSERT_NOT_NULL(recorder_device.get());
    TEST_ASSERT_NOT_NULL(panel_device.get());
    TEST_ASSERT_NOT_NULL(touch_device.get());
    TEST_ASSERT_NOT_NULL(backlight_device.get());
    TEST_ASSERT_NOT_NULL(storage_device.get());

    auto player = player_device->get_interface<hal::AudioCodecPlayerIface>(TestAudioCodecPlayerIface::NAME);
    auto recorder = recorder_device->get_interface<hal::AudioCodecRecorderIface>(TestAudioCodecRecorderIface::NAME);
    auto panel = panel_device->get_interface<hal::DisplayPanelIface>(TestDisplayPanelIface::NAME);
    auto touch = touch_device->get_interface<hal::DisplayTouchIface>(TestDisplayTouchIface::NAME);
    auto backlight = backlight_device->get_interface<hal::DisplayBacklightIface>(TestDisplayBacklightIface::NAME);
    auto storage = storage_device->get_interface<hal::StorageFsIface>(TestStorageFsIface::NAME);

    TEST_ASSERT_NOT_NULL(player.get());
    TEST_ASSERT_NOT_NULL(recorder.get());
    TEST_ASSERT_NOT_NULL(panel.get());
    TEST_ASSERT_NOT_NULL(touch.get());
    TEST_ASSERT_NOT_NULL(backlight.get());
    TEST_ASSERT_NOT_NULL(storage.get());

    TEST_ASSERT_NULL(player_device->get_interface<hal::StorageFsIface>("missing:StorageFs").get());
    TEST_ASSERT_NULL(storage_device->get_interface<hal::AudioCodecPlayerIface>("missing:AudioPlayer").get());

    hal::deinit_all_devices();
}

TEST_CASE("Device: deinit_all_devices clears registered interfaces", "[hal][device]")
{
    hal::deinit_all_devices();
    hal::init_all_devices();
    hal::deinit_all_devices();

    auto [player_name, player] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    auto [recorder_name, recorder] = hal::get_first_interface<hal::AudioCodecRecorderIface>();
    auto [panel_name, panel] = hal::get_first_interface<hal::DisplayPanelIface>();
    auto [touch_name, touch] = hal::get_first_interface<hal::DisplayTouchIface>();
    auto [backlight_name, backlight] = hal::get_first_interface<hal::DisplayBacklightIface>();
    auto [storage_name, storage] = hal::get_first_interface<hal::StorageFsIface>();

    TEST_ASSERT_TRUE(player_name.empty());
    TEST_ASSERT_TRUE(recorder_name.empty());
    TEST_ASSERT_TRUE(panel_name.empty());
    TEST_ASSERT_TRUE(touch_name.empty());
    TEST_ASSERT_TRUE(backlight_name.empty());
    TEST_ASSERT_TRUE(storage_name.empty());

    TEST_ASSERT_NULL(player.get());
    TEST_ASSERT_NULL(recorder.get());
    TEST_ASSERT_NULL(panel.get());
    TEST_ASSERT_NULL(touch.get());
    TEST_ASSERT_NULL(backlight.get());
    TEST_ASSERT_NULL(storage.get());
}

TEST_CASE("Device: init_device handles unknown device name", "[hal][device]")
{
    hal::deinit_all_devices();

    TEST_ASSERT_FALSE(hal::init_device("UnknownDevice"));

    // Should be a no-op and must not crash.
    hal::deinit_device("UnknownDevice");
}
