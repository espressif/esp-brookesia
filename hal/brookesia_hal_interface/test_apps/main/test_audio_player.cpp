/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "boost/format.hpp"
#include "common.hpp"
#include "test_audio_player.hpp"

using namespace esp_brookesia;

namespace esp_brookesia {

bool TestAudioCodecPlayerIface::open(const hal::AudioCodecPlayerIface::Config &config)
{
    BROOKESIA_LOGI("Opened, config=%1%", config);
    return true;
}

void TestAudioCodecPlayerIface::close()
{
    BROOKESIA_LOGI("Closed");
}

bool TestAudioCodecPlayerIface::set_volume(uint8_t volume)
{
    BROOKESIA_LOGI("Set volume to %1%", volume);
    return true;
}

bool TestAudioCodecPlayerIface::get_volume(uint8_t &volume)
{
    BROOKESIA_LOGI("Get volume");
    volume = 75;
    return true;
}

bool TestAudioCodecPlayerIface::mute()
{
    BROOKESIA_LOGI("Mute");
    return true;
}

bool TestAudioCodecPlayerIface::unmute()
{
    BROOKESIA_LOGI("Unmute");
    return true;
}

bool TestAudioCodecPlayerIface::write_data(const uint8_t *data, size_t size)
{
    BROOKESIA_LOGI("Write data, size=%1%", size);
    return true;
}

bool TestAudioPlayerDevice::probe()
{
    return true;
}

bool TestAudioPlayerDevice::on_init()
{
    auto interface = std::make_shared<TestAudioCodecPlayerIface>(hal::AudioCodecPlayerIface::Info {
        .volume_default = 75,
        .volume_min = 0,
        .volume_max = 100,
    });

    interfaces_.emplace(TestAudioCodecPlayerIface::NAME, interface);

    return true;
}

void TestAudioPlayerDevice::on_deinit()
{
    interfaces_.clear();
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestAudioPlayerDevice, std::string(TestAudioPlayerDevice::NAME));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Test Cases //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("AudioCodecPlayerIface: get_device finds plugin after registration", "[hal][device]")
{
    auto device = hal::get_device_by_device_name(TestAudioPlayerDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
}

TEST_CASE("AudioCodecPlayerIface: init_device registers interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: deinit_device releases interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    hal::deinit_device(TestAudioPlayerDevice::NAME);

    auto [name, iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("AudioCodecPlayerIface: get_device returns correct device type", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto device = hal::get_device_by_device_name(TestAudioPlayerDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
    TEST_ASSERT_EQUAL_STRING(TestAudioPlayerDevice::NAME, std::string(device->get_name()).c_str());

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: get_first_interface returns AudioCodecPlayerIface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    TEST_ASSERT_EQUAL_STRING(TestAudioCodecPlayerIface::NAME, name.c_str());

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: get_first_interface returns empty pair when no devices initialized", "[hal][interface]")
{
    auto [name, iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("AudioCodecPlayerIface: get_interfaces returns all AudioCodecPlayerIface instances", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto interfaces = hal::get_interfaces<hal::AudioCodecPlayerIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, interfaces.size());

    for (const auto &[iface_name, iface] : interfaces) {
        TEST_ASSERT_NOT_NULL(iface.get());
        BROOKESIA_LOGI("Found interface: %1%", iface_name);
    }

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: Device::get_interface returns interface from device instance", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto device = hal::get_device_by_device_name(TestAudioPlayerDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::AudioCodecPlayerIface>(TestAudioCodecPlayerIface::NAME);
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: Device::get_interface returns nullptr for missing interface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto device = hal::get_device_by_device_name(TestAudioPlayerDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::AudioCodecPlayerIface>("missing:AudioPlayer");
    TEST_ASSERT_NULL(iface.get());

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: info is correctly propagated", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    const auto &info = iface->get_info();
    TEST_ASSERT_EQUAL_UINT8(75, info.volume_default);
    TEST_ASSERT_EQUAL_UINT8(0, info.volume_min);
    TEST_ASSERT_EQUAL_UINT8(100, info.volume_max);

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: open/close/set_volume/get_volume/write_data", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    TEST_ASSERT_TRUE(iface->open(hal::AudioCodecPlayerIface::Config {
        .bits = 16,
        .channels = 1,
        .sample_rate = 16000,
    }));
    TEST_ASSERT_TRUE(iface->set_volume(50));

    uint8_t vol = 0;
    TEST_ASSERT_TRUE(iface->get_volume(vol));
    TEST_ASSERT_EQUAL_UINT8(75, vol);

    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    TEST_ASSERT_TRUE(iface->write_data(data, sizeof(data)));

    iface->close();

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: repeated init/deinit cycles work correctly", "[hal][device]")
{
    for (int i = 0; i < 3; i++) {
        BROOKESIA_LOGI("Cycle %1%", i + 1);

        TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

        auto [name, iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
        TEST_ASSERT_NOT_NULL(iface.get());
        TEST_ASSERT_TRUE(iface->open(hal::AudioCodecPlayerIface::Config {
            .bits = 16,
            .channels = 1,
            .sample_rate = 16000,
        }));
        iface->close();

        hal::deinit_device(TestAudioPlayerDevice::NAME);

        auto [name2, iface2] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
        TEST_ASSERT_NULL(iface2.get());
    }
}
