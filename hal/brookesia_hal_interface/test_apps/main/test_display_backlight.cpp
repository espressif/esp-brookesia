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

namespace {
bool g_light_on_off_supported = true;
}

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

bool TestDisplayBacklightIface::set_light_on_off(bool on)
{
    BROOKESIA_LOGI("Turn %1%", on ? "on" : "off");
    if (!light_on_off_supported_) {
        return false;
    }
    is_light_on_ = on;
    return true;
}

bool TestDisplayBacklightDevice::probe()
{
    return true;
}

std::vector<hal::InterfaceSpec> TestDisplayBacklightDevice::get_interface_specs() const
{
    return {{hal::display::BacklightIface::NAME, TestDisplayBacklightIface::NAME}};
}

bool TestDisplayBacklightDevice::on_init()
{
    auto interface = std::make_shared<TestDisplayBacklightIface>(g_light_on_off_supported);
    interfaces_.emplace(TestDisplayBacklightIface::NAME, interface);
    return true;
}

void TestDisplayBacklightDevice::on_deinit()
{
    interfaces_.clear();
}

void TestDisplayBacklightDevice::set_light_on_off_supported(bool light_on_off_supported)
{
    g_light_on_off_supported = light_on_off_supported;
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestDisplayBacklightDevice, std::string(TestDisplayBacklightDevice::NAME));


TEST_CASE("display::BacklightIface: acquire and control brightness", "[hal][interface]")
{
    TestDisplayBacklightDevice::set_light_on_off_supported(true);
    auto handle = hal::acquire_first_interface<hal::display::BacklightIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_EQUAL_STRING(TestDisplayBacklightIface::NAME, std::string(handle.instance_name()).c_str());

    auto test_iface = std::dynamic_pointer_cast<TestDisplayBacklightIface>(handle.get());
    TEST_ASSERT_NOT_NULL(test_iface.get());
    TEST_ASSERT_TRUE(test_iface->set_brightness(42));
    uint8_t brightness = 0;
    TEST_ASSERT_TRUE(test_iface->get_brightness(brightness));
    TEST_ASSERT_EQUAL_UINT8(42, brightness);
    TEST_ASSERT_TRUE(test_iface->is_light_on_off_supported());
    TEST_ASSERT_TRUE(test_iface->set_light_on_off(true));
    TEST_ASSERT_TRUE(test_iface->is_light_on());
}

TEST_CASE("display::BacklightIface: acquire by instance and unsupported light switch", "[hal][interface]")
{
    TestDisplayBacklightDevice::set_light_on_off_supported(false);
    auto handle = hal::acquire_interface<hal::display::BacklightIface>(TestDisplayBacklightIface::NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(handle));

    auto test_iface = std::dynamic_pointer_cast<TestDisplayBacklightIface>(handle.get());
    TEST_ASSERT_NOT_NULL(test_iface.get());
    TEST_ASSERT_FALSE(test_iface->is_light_on_off_supported());
    TEST_ASSERT_FALSE(test_iface->set_light_on_off(true));

    auto handles = hal::acquire_interfaces<hal::display::BacklightIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, handles.size());
}
