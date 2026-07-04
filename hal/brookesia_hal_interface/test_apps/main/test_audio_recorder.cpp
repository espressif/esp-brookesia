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

std::vector<hal::InterfaceSpec> TestAudioRecorderDevice::get_interface_specs() const
{
    return {{hal::audio::CodecRecorderIface::NAME, TestAudioCodecRecorderIface::NAME}};
}

bool TestAudioRecorderDevice::on_init()
{
    auto interface = std::make_shared<TestAudioCodecRecorderIface>(hal::audio::CodecRecorderIface::Info {
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

TEST_CASE("audio::CodecRecorderIface: acquire and record data", "[hal][interface]")
{
    auto handle = hal::acquire_first_interface<hal::audio::CodecRecorderIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_EQUAL_STRING(TestAudioCodecRecorderIface::NAME, std::string(handle.instance_name()).c_str());

    auto iface = handle.get();
    TEST_ASSERT_NOT_NULL(iface.get());
    const auto &info = iface->get_info();
    TEST_ASSERT_EQUAL_UINT8(16, info.bits);
    TEST_ASSERT_EQUAL_UINT8(2, info.channels);
    TEST_ASSERT_EQUAL_UINT32(16000, info.sample_rate);

    TEST_ASSERT_TRUE(iface->open());
    uint8_t data[4] = {};
    TEST_ASSERT_TRUE(iface->read_data(data, sizeof(data)));
    for (const auto value : data) {
        TEST_ASSERT_EQUAL_HEX8(0x5A, value);
    }
    TEST_ASSERT_TRUE(iface->set_general_gain(2.0F));
    TEST_ASSERT_TRUE(iface->set_channel_gains({{0, 1.5F}}));
    iface->close();
}

TEST_CASE("audio::CodecRecorderIface: acquire by instance and list handles", "[hal][interface]")
{
    auto handle = hal::acquire_interface<hal::audio::CodecRecorderIface>(TestAudioCodecRecorderIface::NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(handle));

    auto handles = hal::acquire_interfaces<hal::audio::CodecRecorderIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, handles.size());
    TEST_ASSERT_TRUE(hal::has_interface<hal::audio::CodecRecorderIface>());
}
