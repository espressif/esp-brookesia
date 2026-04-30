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

bool TestAudioCodecPlayerIface::open(const hal::AudioCodecPlayerIface::Config &config)
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

namespace {

std::shared_ptr<TestAudioCodecPlayerIface> get_player_iface_or_null()
{
    auto [name, iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    TEST_ASSERT_FALSE(name.empty());
    TEST_ASSERT_NOT_NULL(iface.get());

    auto test_iface = std::dynamic_pointer_cast<TestAudioCodecPlayerIface>(iface);
    TEST_ASSERT_NOT_NULL(test_iface.get());

    return test_iface;
}

} // namespace

TEST_CASE("AudioCodecPlayerIface: get_device finds plugin after registration", "[hal][device]")
{
    auto device = hal::get_device_by_device_name(TestAudioPlayerDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
}

TEST_CASE("AudioCodecPlayerIface: init_device registers interfaces", "[hal][device]")
{
    TestAudioPlayerDevice::set_pa_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: deinit_device releases interfaces", "[hal][device]")
{
    TestAudioPlayerDevice::set_pa_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    hal::deinit_device(TestAudioPlayerDevice::NAME);

    auto [name, iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("AudioCodecPlayerIface: get_device returns correct device type", "[hal][device]")
{
    TestAudioPlayerDevice::set_pa_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto device = hal::get_device_by_device_name(TestAudioPlayerDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
    TEST_ASSERT_EQUAL_STRING(TestAudioPlayerDevice::NAME, std::string(device->get_name()).c_str());

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: get_first_interface returns AudioCodecPlayerIface", "[hal][interface]")
{
    TestAudioPlayerDevice::set_pa_supported(true);
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

TEST_CASE("AudioCodecPlayerIface: get_interfaces returns all instances", "[hal][interface]")
{
    TestAudioPlayerDevice::set_pa_supported(true);
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
    TestAudioPlayerDevice::set_pa_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto device = hal::get_device_by_device_name(TestAudioPlayerDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::AudioCodecPlayerIface>(TestAudioCodecPlayerIface::NAME);
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: Device::get_interface returns nullptr for missing interface", "[hal][interface]")
{
    TestAudioPlayerDevice::set_pa_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto device = hal::get_device_by_device_name(TestAudioPlayerDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::AudioCodecPlayerIface>("missing:AudioPlayer");
    TEST_ASSERT_NULL(iface.get());

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: open close set_volume write_data", "[hal][interface]")
{
    TestAudioPlayerDevice::set_pa_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto test_iface = get_player_iface_or_null();

    TEST_ASSERT_TRUE(test_iface->open(hal::AudioCodecPlayerIface::Config {
        .bits = 16,
        .channels = 1,
        .sample_rate = 16000,
    }));
    TEST_ASSERT_TRUE(test_iface->is_opened());
    TEST_ASSERT_TRUE(test_iface->set_volume(50));
    TEST_ASSERT_EQUAL_UINT8(50, test_iface->get_last_volume());

    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    TEST_ASSERT_TRUE(test_iface->write_data(data, sizeof(data)));
    TEST_ASSERT_EQUAL(sizeof(data), test_iface->get_total_bytes_written());

    test_iface->close();
    TEST_ASSERT_FALSE(test_iface->is_opened());

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: PA control supported path", "[hal][interface]")
{
    TestAudioPlayerDevice::set_pa_supported(true);
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto test_iface = get_player_iface_or_null();
    TEST_ASSERT_TRUE(test_iface->is_pa_on_off_supported());
    TEST_ASSERT_TRUE(test_iface->is_pa_on());
    TEST_ASSERT_TRUE(test_iface->set_pa_on_off(false));
    TEST_ASSERT_FALSE(test_iface->is_pa_on());
    TEST_ASSERT_TRUE(test_iface->set_pa_on_off(true));
    TEST_ASSERT_TRUE(test_iface->is_pa_on());

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: PA control unsupported path", "[hal][interface]")
{
    TestAudioPlayerDevice::set_pa_supported(false);
    TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

    auto test_iface = get_player_iface_or_null();
    TEST_ASSERT_FALSE(test_iface->is_pa_on_off_supported());
    TEST_ASSERT_TRUE(test_iface->is_pa_on());
    TEST_ASSERT_FALSE(test_iface->set_pa_on_off(false));
    TEST_ASSERT_TRUE(test_iface->is_pa_on());

    hal::deinit_device(TestAudioPlayerDevice::NAME);
}

TEST_CASE("AudioCodecPlayerIface: repeated init/deinit cycles work correctly", "[hal][device]")
{
    TestAudioPlayerDevice::set_pa_supported(true);
    for (int i = 0; i < 3; i++) {
        BROOKESIA_LOGI("Cycle %1%", i + 1);

        TEST_ASSERT_TRUE(hal::init_device(TestAudioPlayerDevice::NAME));

        auto test_iface = get_player_iface_or_null();
        TEST_ASSERT_TRUE(test_iface->open(hal::AudioCodecPlayerIface::Config {
            .bits = 16,
            .channels = 1,
            .sample_rate = 16000,
        }));
        test_iface->close();

        hal::deinit_device(TestAudioPlayerDevice::NAME);

        auto [name, iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
        TEST_ASSERT_NULL(iface.get());
        TEST_ASSERT_TRUE(name.empty());
    }
}
