/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.hpp"
#include "test_audio_player.hpp"

using namespace esp_brookesia;

namespace esp_brookesia {

namespace {
bool g_pa_supported = true;
}

bool TestAudioCodecPlayerIface::open(const hal::audio::CodecPlayerIface::Config &config)
{
    BROOKESIA_LOGI("Opened, config=%1%", config);
    is_opened_ = true;
    total_bytes_written_ = 0;
    return true;
}

void TestAudioCodecPlayerIface::close()
{
    BROOKESIA_LOGI("Closed");
    is_opened_ = false;
}

bool TestAudioCodecPlayerIface::set_volume(uint8_t volume)
{
    BROOKESIA_LOGI("Set volume to %1%", volume);
    last_volume_ = volume;
    return true;
}

bool TestAudioCodecPlayerIface::write_data(const uint8_t *data, size_t size)
{
    BROOKESIA_LOGI("Write data, size=%1%", size);
    if (data == nullptr) {
        return false;
    }
    total_bytes_written_ += size;
    return true;
}

bool TestAudioCodecPlayerIface::set_pa_on_off(bool on)
{
    BROOKESIA_LOGI("Set PA %1%", on ? "on" : "off");
    if (!pa_supported_) {
        return false;
    }
    pa_on_ = on;
    return true;
}

bool TestAudioPlayerDevice::probe()
{
    return true;
}

std::vector<hal::InterfaceSpec> TestAudioPlayerDevice::get_interface_specs() const
{
    return {{hal::audio::CodecPlayerIface::NAME, TestAudioCodecPlayerIface::NAME}};
}

bool TestAudioPlayerDevice::on_init()
{
    auto interface = std::make_shared<TestAudioCodecPlayerIface>(g_pa_supported);
    interfaces_.emplace(TestAudioCodecPlayerIface::NAME, interface);
    return true;
}

void TestAudioPlayerDevice::on_deinit()
{
    interfaces_.clear();
}

void TestAudioPlayerDevice::set_pa_supported(bool pa_supported)
{
    g_pa_supported = pa_supported;
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestAudioPlayerDevice, std::string(TestAudioPlayerDevice::NAME));


TEST_CASE("audio::CodecPlayerIface: acquire and play data", "[hal][interface]")
{
    TestAudioPlayerDevice::set_pa_supported(true);
    auto handle = hal::acquire_first_interface<hal::audio::CodecPlayerIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_EQUAL_STRING(TestAudioCodecPlayerIface::NAME, std::string(handle.instance_name()).c_str());

    auto iface = handle.get();
    auto test_iface = std::dynamic_pointer_cast<TestAudioCodecPlayerIface>(iface);
    TEST_ASSERT_NOT_NULL(test_iface.get());

    TEST_ASSERT_TRUE(test_iface->open({.bits = 16, .channels = 1, .sample_rate = 16000}));
    TEST_ASSERT_TRUE(test_iface->is_opened());
    TEST_ASSERT_TRUE(test_iface->set_volume(50));
    TEST_ASSERT_EQUAL_UINT8(50, test_iface->get_last_volume());

    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    TEST_ASSERT_TRUE(test_iface->write_data(data, sizeof(data)));
    TEST_ASSERT_EQUAL(sizeof(data), test_iface->get_total_bytes_written());

    TEST_ASSERT_TRUE(test_iface->is_pa_on_off_supported());
    TEST_ASSERT_TRUE(test_iface->set_pa_on_off(false));
    TEST_ASSERT_FALSE(test_iface->is_pa_on());
    test_iface->close();
    TEST_ASSERT_FALSE(test_iface->is_opened());
}

TEST_CASE("audio::CodecPlayerIface: acquire by instance and unsupported PA path", "[hal][interface]")
{
    TestAudioPlayerDevice::set_pa_supported(false);
    auto handle = hal::acquire_interface<hal::audio::CodecPlayerIface>(TestAudioCodecPlayerIface::NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(handle));

    auto test_iface = std::dynamic_pointer_cast<TestAudioCodecPlayerIface>(handle.get());
    TEST_ASSERT_NOT_NULL(test_iface.get());
    TEST_ASSERT_FALSE(test_iface->is_pa_on_off_supported());
    TEST_ASSERT_FALSE(test_iface->set_pa_on_off(false));

    auto handles = hal::acquire_interfaces<hal::audio::CodecPlayerIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, handles.size());
}
