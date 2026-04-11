/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include <algorithm>
#include "common.hpp"
#include "test_audio_recorder.hpp"

using namespace esp_brookesia;

namespace esp_brookesia {

bool TestAudioCodecRecorderIface::open()
{
    BROOKESIA_LOGI("Opened");
    return true;
}

void TestAudioCodecRecorderIface::close()
{
    BROOKESIA_LOGI("Closed");
}

bool TestAudioCodecRecorderIface::read_data(uint8_t *data, size_t size)
{
    BROOKESIA_LOGI("Read data, size=%1%", size);
    if (data != nullptr) {
        std::fill_n(data, size, 0x5A);
    }
    return true;
}

bool TestAudioCodecRecorderIface::set_general_gain(float gain)
{
    BROOKESIA_LOGI("Set general gain: %f", gain);
    // Update the general gain in the info structure
    const_cast<Info &>(get_info()).general_gain = gain;
    return true;
}

bool TestAudioCodecRecorderIface::set_channel_gains(const std::map<uint8_t, float> &gains)
{
    BROOKESIA_LOGI("Set channel gains, count: %d", gains.size());
    // Update the channel gains in the info structure
    const_cast<Info &>(get_info()).channel_gains = gains;
    return true;
}

bool TestAudioRecorderDevice::probe()
{
    return true;
}

bool TestAudioRecorderDevice::on_init()
{
    auto interface = std::make_shared<TestAudioCodecRecorderIface>(hal::AudioCodecRecorderIface::Info {
        .bits = 16,
        .channels = 2,
        .sample_rate = 16000,
        .mic_layout = "LR",
        .general_gain = 1.5f,
        .channel_gains = {
            {0, 1.0f},
            {1, 1.25f},
        },
    });

    interfaces_.emplace(TestAudioCodecRecorderIface::NAME, interface);

    return true;
}

void TestAudioRecorderDevice::on_deinit()
{
    interfaces_.clear();
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestAudioRecorderDevice, std::string(TestAudioRecorderDevice::NAME));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Test Cases //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("AudioCodecRecorderIface: get_device finds plugin after registration", "[hal][device]")
{
    auto device = hal::get_device_by_device_name(TestAudioRecorderDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
}

TEST_CASE("AudioCodecRecorderIface: init_device registers interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioRecorderDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::AudioCodecRecorderIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    hal::deinit_device(TestAudioRecorderDevice::NAME);
}

TEST_CASE("AudioCodecRecorderIface: deinit_device releases interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioRecorderDevice::NAME));

    hal::deinit_device(TestAudioRecorderDevice::NAME);

    auto [name, iface] = hal::get_first_interface<hal::AudioCodecRecorderIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("AudioCodecRecorderIface: get_device returns correct device type", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioRecorderDevice::NAME));

    auto device = hal::get_device_by_device_name(TestAudioRecorderDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
    TEST_ASSERT_EQUAL_STRING(TestAudioRecorderDevice::NAME, std::string(device->get_name()).c_str());

    hal::deinit_device(TestAudioRecorderDevice::NAME);
}

TEST_CASE("AudioCodecRecorderIface: get_first_interface returns AudioCodecRecorderIface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioRecorderDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::AudioCodecRecorderIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    TEST_ASSERT_EQUAL_STRING(TestAudioCodecRecorderIface::NAME, name.c_str());

    hal::deinit_device(TestAudioRecorderDevice::NAME);
}

TEST_CASE("AudioCodecRecorderIface: get_first_interface returns empty pair when no devices initialized", "[hal][interface]")
{
    auto [name, iface] = hal::get_first_interface<hal::AudioCodecRecorderIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("AudioCodecRecorderIface: get_interfaces returns all AudioCodecRecorderIface instances", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioRecorderDevice::NAME));

    auto interfaces = hal::get_interfaces<hal::AudioCodecRecorderIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, interfaces.size());

    for (const auto &[iface_name, iface] : interfaces) {
        TEST_ASSERT_NOT_NULL(iface.get());
        BROOKESIA_LOGI("Found interface: %1%", iface_name);
    }

    hal::deinit_device(TestAudioRecorderDevice::NAME);
}

TEST_CASE("AudioCodecRecorderIface: Device::get_interface returns interface from device instance", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioRecorderDevice::NAME));

    auto device = hal::get_device_by_device_name(TestAudioRecorderDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::AudioCodecRecorderIface>(TestAudioCodecRecorderIface::NAME);
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::deinit_device(TestAudioRecorderDevice::NAME);
}

TEST_CASE("AudioCodecRecorderIface: Device::get_interface returns nullptr for missing interface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioRecorderDevice::NAME));

    auto device = hal::get_device_by_device_name(TestAudioRecorderDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::AudioCodecRecorderIface>("missing:AudioRecorder");
    TEST_ASSERT_NULL(iface.get());

    hal::deinit_device(TestAudioRecorderDevice::NAME);
}

TEST_CASE("AudioCodecRecorderIface: info is correctly propagated", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioRecorderDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::AudioCodecRecorderIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    const auto &info = iface->get_info();
    TEST_ASSERT_EQUAL_UINT8(16, info.bits);
    TEST_ASSERT_EQUAL_UINT8(2, info.channels);
    TEST_ASSERT_EQUAL_UINT32(16000, info.sample_rate);
    TEST_ASSERT_EQUAL_STRING("LR", info.mic_layout.c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.5f, info.general_gain);
    TEST_ASSERT_EQUAL(2, info.channel_gains.size());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, info.channel_gains.at(0));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.25f, info.channel_gains.at(1));

    hal::deinit_device(TestAudioRecorderDevice::NAME);
}

TEST_CASE("AudioCodecRecorderIface: open/close/read_data", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestAudioRecorderDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::AudioCodecRecorderIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    TEST_ASSERT_TRUE(iface->open());

    uint8_t data[4] = {};
    TEST_ASSERT_TRUE(iface->read_data(data, sizeof(data)));
    for (const auto value : data) {
        TEST_ASSERT_EQUAL_HEX8(0x5A, value);
    }

    iface->close();

    hal::deinit_device(TestAudioRecorderDevice::NAME);
}

TEST_CASE("AudioCodecRecorderIface: repeated init/deinit cycles work correctly", "[hal][device]")
{
    for (int i = 0; i < 3; i++) {
        BROOKESIA_LOGI("Cycle %1%", i + 1);

        TEST_ASSERT_TRUE(hal::init_device(TestAudioRecorderDevice::NAME));

        auto [name, iface] = hal::get_first_interface<hal::AudioCodecRecorderIface>();
        TEST_ASSERT_NOT_NULL(iface.get());
        TEST_ASSERT_TRUE(iface->open());
        iface->close();

        hal::deinit_device(TestAudioRecorderDevice::NAME);

        auto [name2, iface2] = hal::get_first_interface<hal::AudioCodecRecorderIface>();
        TEST_ASSERT_NULL(iface2.get());
    }
}
