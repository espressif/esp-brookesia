/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.hpp"
#include "test_display_backlight.hpp"

using namespace esp_brookesia;

namespace esp_brookesia {

bool TestDisplayBacklightIface::set_brightness(uint8_t percent)
{
    BROOKESIA_LOGI("Set brightness to %1%", percent);
    brightness_ = percent;
    return true;
}

bool TestDisplayBacklightIface::get_brightness(uint8_t &percent)
{
    BROOKESIA_LOGI("Get brightness");
    percent = brightness_;
    return true;
}

bool TestDisplayBacklightIface::turn_on()
{
    BROOKESIA_LOGI("Turn on");
    return true;
}

bool TestDisplayBacklightIface::turn_off()
{
    BROOKESIA_LOGI("Turn off");
    return true;
}

bool TestDisplayBacklightDevice::probe()
{
    return true;
}

bool TestDisplayBacklightDevice::on_init()
{
    auto interface = std::make_shared<TestDisplayBacklightIface>(hal::DisplayBacklightIface::Info {
        .brightness_default = 80,
        .brightness_min = 5,
        .brightness_max = 100,
    });

    interfaces_.emplace(TestDisplayBacklightIface::NAME, interface);

    return true;
}

void TestDisplayBacklightDevice::on_deinit()
{
    interfaces_.clear();
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestDisplayBacklightDevice, std::string(TestDisplayBacklightDevice::NAME));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Test Cases //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("DisplayBacklightIface: get_device finds plugin after registration", "[hal][device]")
{
    auto device = hal::get_device_by_device_name(TestDisplayBacklightDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
}

TEST_CASE("DisplayBacklightIface: init_device registers interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayBacklightDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayBacklightIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    hal::deinit_device(TestDisplayBacklightDevice::NAME);
}

TEST_CASE("DisplayBacklightIface: deinit_device releases interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayBacklightDevice::NAME));

    hal::deinit_device(TestDisplayBacklightDevice::NAME);

    auto [name, iface] = hal::get_first_interface<hal::DisplayBacklightIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("DisplayBacklightIface: get_device returns correct device type", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayBacklightDevice::NAME));

    auto device = hal::get_device_by_device_name(TestDisplayBacklightDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
    TEST_ASSERT_EQUAL_STRING(TestDisplayBacklightDevice::NAME, std::string(device->get_name()).c_str());

    hal::deinit_device(TestDisplayBacklightDevice::NAME);
}

TEST_CASE("DisplayBacklightIface: get_first_interface returns DisplayBacklightIface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayBacklightDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayBacklightIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    TEST_ASSERT_EQUAL_STRING(TestDisplayBacklightIface::NAME, name.c_str());

    hal::deinit_device(TestDisplayBacklightDevice::NAME);
}

TEST_CASE("DisplayBacklightIface: get_first_interface returns empty pair when no devices initialized", "[hal][interface]")
{
    auto [name, iface] = hal::get_first_interface<hal::DisplayBacklightIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("DisplayBacklightIface: get_interfaces returns all DisplayBacklightIface instances", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayBacklightDevice::NAME));

    auto interfaces = hal::get_interfaces<hal::DisplayBacklightIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, interfaces.size());

    for (const auto &[iface_name, iface] : interfaces) {
        TEST_ASSERT_NOT_NULL(iface.get());
        BROOKESIA_LOGI("Found interface: %1%", iface_name);
    }

    hal::deinit_device(TestDisplayBacklightDevice::NAME);
}

TEST_CASE("DisplayBacklightIface: Device::get_interface returns interface from device instance", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayBacklightDevice::NAME));

    auto device = hal::get_device_by_device_name(TestDisplayBacklightDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::DisplayBacklightIface>(TestDisplayBacklightIface::NAME);
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::deinit_device(TestDisplayBacklightDevice::NAME);
}

TEST_CASE("DisplayBacklightIface: Device::get_interface returns nullptr for missing interface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayBacklightDevice::NAME));

    auto device = hal::get_device_by_device_name(TestDisplayBacklightDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::DisplayBacklightIface>("missing:DisplayBacklight");
    TEST_ASSERT_NULL(iface.get());

    hal::deinit_device(TestDisplayBacklightDevice::NAME);
}

TEST_CASE("DisplayBacklightIface: info is correctly propagated", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayBacklightDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayBacklightIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    const auto &info = iface->get_info();
    TEST_ASSERT_EQUAL_UINT8(80, info.brightness_default);
    TEST_ASSERT_EQUAL_UINT8(5, info.brightness_min);
    TEST_ASSERT_EQUAL_UINT8(100, info.brightness_max);

    hal::deinit_device(TestDisplayBacklightDevice::NAME);
}

TEST_CASE("DisplayBacklightIface: set_brightness/get_brightness", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayBacklightDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayBacklightIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    TEST_ASSERT_TRUE(iface->set_brightness(42));

    uint8_t brightness = 0;
    TEST_ASSERT_TRUE(iface->get_brightness(brightness));
    TEST_ASSERT_EQUAL_UINT8(42, brightness);

    hal::deinit_device(TestDisplayBacklightDevice::NAME);
}

TEST_CASE("DisplayBacklightIface: repeated init/deinit cycles work correctly", "[hal][device]")
{
    for (int i = 0; i < 3; i++) {
        BROOKESIA_LOGI("Cycle %1%", i + 1);

        TEST_ASSERT_TRUE(hal::init_device(TestDisplayBacklightDevice::NAME));

        auto [name, iface] = hal::get_first_interface<hal::DisplayBacklightIface>();
        TEST_ASSERT_NOT_NULL(iface.get());
        TEST_ASSERT_TRUE(iface->set_brightness(25));

        hal::deinit_device(TestDisplayBacklightDevice::NAME);

        auto [name2, iface2] = hal::get_first_interface<hal::DisplayBacklightIface>();
        TEST_ASSERT_NULL(iface2.get());
    }
}
