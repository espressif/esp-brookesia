/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.hpp"
#include "test_display_panel.hpp"

using namespace esp_brookesia;

namespace esp_brookesia {

bool TestDisplayPanelIface::draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data)
{
    BROOKESIA_LOGI("Draw bitmap (%1%, %2%) -> (%3%, %4%)", x1, y1, x2, y2);
    return (x2 > x1) && (y2 > y1) && (data != nullptr);
}

bool TestDisplayPanelIface::get_driver_specific(DriverSpecific &specific)
{
    BROOKESIA_LOGI("Get driver specific");
    specific.io_handle = reinterpret_cast<void *>(0x12345678);
    specific.panel_handle = reinterpret_cast<void *>(0x87654321);
    return true;
}

bool TestDisplayPanelDevice::probe()
{
    return true;
}

bool TestDisplayPanelDevice::on_init()
{
    auto interface = std::make_shared<TestDisplayPanelIface>(hal::DisplayPanelIface::Info {
        .h_res = 320,
        .v_res = 240,
    });

    interfaces_.emplace(TestDisplayPanelIface::NAME, interface);

    return true;
}

void TestDisplayPanelDevice::on_deinit()
{
    interfaces_.clear();
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestDisplayPanelDevice, std::string(TestDisplayPanelDevice::NAME));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Test Cases //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("DisplayPanelIface: get_device finds plugin after registration", "[hal][device]")
{
    auto device = hal::get_device_by_device_name(TestDisplayPanelDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
}

TEST_CASE("DisplayPanelIface: init_device registers interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayPanelDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayPanelIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    hal::deinit_device(TestDisplayPanelDevice::NAME);
}

TEST_CASE("DisplayPanelIface: deinit_device releases interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayPanelDevice::NAME));

    hal::deinit_device(TestDisplayPanelDevice::NAME);

    auto [name, iface] = hal::get_first_interface<hal::DisplayPanelIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("DisplayPanelIface: get_device returns correct device type", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayPanelDevice::NAME));

    auto device = hal::get_device_by_device_name(TestDisplayPanelDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
    TEST_ASSERT_EQUAL_STRING(TestDisplayPanelDevice::NAME, std::string(device->get_name()).c_str());

    hal::deinit_device(TestDisplayPanelDevice::NAME);
}

TEST_CASE("DisplayPanelIface: get_first_interface returns DisplayPanelIface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayPanelDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayPanelIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    TEST_ASSERT_EQUAL_STRING(TestDisplayPanelIface::NAME, name.c_str());

    hal::deinit_device(TestDisplayPanelDevice::NAME);
}

TEST_CASE("DisplayPanelIface: get_first_interface returns empty pair when no devices initialized", "[hal][interface]")
{
    auto [name, iface] = hal::get_first_interface<hal::DisplayPanelIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("DisplayPanelIface: get_interfaces returns all DisplayPanelIface instances", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayPanelDevice::NAME));

    auto interfaces = hal::get_interfaces<hal::DisplayPanelIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, interfaces.size());

    for (const auto &[iface_name, iface] : interfaces) {
        TEST_ASSERT_NOT_NULL(iface.get());
        BROOKESIA_LOGI("Found interface: %1%", iface_name);
    }

    hal::deinit_device(TestDisplayPanelDevice::NAME);
}

TEST_CASE("DisplayPanelIface: Device::get_interface returns interface from device instance", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayPanelDevice::NAME));

    auto device = hal::get_device_by_device_name(TestDisplayPanelDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::DisplayPanelIface>(TestDisplayPanelIface::NAME);
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::deinit_device(TestDisplayPanelDevice::NAME);
}

TEST_CASE("DisplayPanelIface: Device::get_interface returns nullptr for missing interface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayPanelDevice::NAME));

    auto device = hal::get_device_by_device_name(TestDisplayPanelDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::DisplayPanelIface>("missing:DisplayPanel");
    TEST_ASSERT_NULL(iface.get());

    hal::deinit_device(TestDisplayPanelDevice::NAME);
}

TEST_CASE("DisplayPanelIface: info is correctly propagated", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayPanelDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayPanelIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    const auto &info = iface->get_info();
    TEST_ASSERT_EQUAL_UINT16(320, info.h_res);
    TEST_ASSERT_EQUAL_UINT16(240, info.v_res);

    hal::deinit_device(TestDisplayPanelDevice::NAME);
}

TEST_CASE("DisplayPanelIface: draw_bitmap", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayPanelDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayPanelIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    const uint8_t bitmap[] = {0x11, 0x22, 0x33, 0x44};
    TEST_ASSERT_TRUE(iface->draw_bitmap(0, 0, 2, 2, bitmap));

    hal::deinit_device(TestDisplayPanelDevice::NAME);
}

TEST_CASE("DisplayPanelIface: get_driver_specific", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayPanelDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayPanelIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::DisplayPanelIface::DriverSpecific specific;
    TEST_ASSERT_TRUE(iface->get_driver_specific(specific));
    TEST_ASSERT_NOT_NULL(specific.io_handle);
    TEST_ASSERT_NOT_NULL(specific.panel_handle);
}

TEST_CASE("DisplayPanelIface: repeated init/deinit cycles work correctly", "[hal][device]")
{
    for (int i = 0; i < 3; i++) {
        BROOKESIA_LOGI("Cycle %1%", i + 1);

        TEST_ASSERT_TRUE(hal::init_device(TestDisplayPanelDevice::NAME));

        auto [name, iface] = hal::get_first_interface<hal::DisplayPanelIface>();
        TEST_ASSERT_NOT_NULL(iface.get());
        const uint8_t bitmap[] = {0xAB, 0xCD};
        TEST_ASSERT_TRUE(iface->draw_bitmap(0, 0, 1, 1, bitmap));

        hal::deinit_device(TestDisplayPanelDevice::NAME);

        auto [name2, iface2] = hal::get_first_interface<hal::DisplayPanelIface>();
        TEST_ASSERT_NULL(iface2.get());
    }
}
