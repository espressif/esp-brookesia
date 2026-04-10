/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.hpp"
#include "test_display_touch.hpp"

using namespace esp_brookesia;

namespace esp_brookesia {

bool TestDisplayTouchIface::read_points(std::vector<hal::DisplayTouchIface::Point> &points)
{
    BROOKESIA_LOGI("Read points");
    points = {
        hal::DisplayTouchIface::Point{.x = 12, .y = 34, .pressure = 56},
        hal::DisplayTouchIface::Point{.x = 78, .y = 90, .pressure = 12},
    };
    return true;
}

bool TestDisplayTouchIface::register_interrupt_handler(hal::DisplayTouchIface::InterruptHandler handler)
{
    interrupt_handler_ = std::move(handler);
    return true;
}

bool TestDisplayTouchIface::get_driver_specific(hal::DisplayTouchIface::DriverSpecific &specific)
{
    BROOKESIA_LOGI("Get driver specific");
    specific.io_handle = reinterpret_cast<void *>(0x12345678);
    specific.touch_handle = reinterpret_cast<void *>(0x87654321);
    return true;
}

bool TestDisplayTouchDevice::probe()
{
    return true;
}

bool TestDisplayTouchDevice::on_init()
{
    auto interface = std::make_shared<TestDisplayTouchIface>(hal::DisplayTouchIface::Info {
        .x_max = 320,
        .y_max = 240,
        .operation_mode = hal::DisplayTouchIface::OperationMode::Polling
    });

    interfaces_.emplace(TestDisplayTouchIface::NAME, interface);

    return true;
}

void TestDisplayTouchDevice::on_deinit()
{
    interfaces_.clear();
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestDisplayTouchDevice, std::string(TestDisplayTouchDevice::NAME));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Test Cases //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("DisplayTouchIface: get_device finds plugin after registration", "[hal][device]")
{
    auto device = hal::get_device_by_device_name(TestDisplayTouchDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
}

TEST_CASE("DisplayTouchIface: init_device registers interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayTouchDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayTouchIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    hal::deinit_device(TestDisplayTouchDevice::NAME);
}

TEST_CASE("DisplayTouchIface: deinit_device releases interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayTouchDevice::NAME));

    hal::deinit_device(TestDisplayTouchDevice::NAME);

    auto [name, iface] = hal::get_first_interface<hal::DisplayTouchIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("DisplayTouchIface: get_device returns correct device type", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayTouchDevice::NAME));

    auto device = hal::get_device_by_device_name(TestDisplayTouchDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
    TEST_ASSERT_EQUAL_STRING(TestDisplayTouchDevice::NAME, std::string(device->get_name()).c_str());

    hal::deinit_device(TestDisplayTouchDevice::NAME);
}

TEST_CASE("DisplayTouchIface: get_first_interface returns DisplayTouchIface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayTouchDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayTouchIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    TEST_ASSERT_EQUAL_STRING(TestDisplayTouchIface::NAME, name.c_str());

    hal::deinit_device(TestDisplayTouchDevice::NAME);
}

TEST_CASE("DisplayTouchIface: get_first_interface returns empty pair when no devices initialized", "[hal][interface]")
{
    auto [name, iface] = hal::get_first_interface<hal::DisplayTouchIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("DisplayTouchIface: get_interfaces returns all DisplayTouchIface instances", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayTouchDevice::NAME));

    auto interfaces = hal::get_interfaces<hal::DisplayTouchIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, interfaces.size());

    for (const auto &[iface_name, iface] : interfaces) {
        TEST_ASSERT_NOT_NULL(iface.get());
        BROOKESIA_LOGI("Found interface: %1%", iface_name);
    }

    hal::deinit_device(TestDisplayTouchDevice::NAME);
}

TEST_CASE("DisplayTouchIface: Device::get_interface returns interface from device instance", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayTouchDevice::NAME));

    auto device = hal::get_device_by_device_name(TestDisplayTouchDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::DisplayTouchIface>(TestDisplayTouchIface::NAME);
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::deinit_device(TestDisplayTouchDevice::NAME);
}

TEST_CASE("DisplayTouchIface: Device::get_interface returns nullptr for missing interface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayTouchDevice::NAME));

    auto device = hal::get_device_by_device_name(TestDisplayTouchDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::DisplayTouchIface>("missing:DisplayTouch");
    TEST_ASSERT_NULL(iface.get());

    hal::deinit_device(TestDisplayTouchDevice::NAME);
}

TEST_CASE("DisplayTouchIface: info is correctly propagated", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayTouchDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayTouchIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    const auto &info = iface->get_info();
    TEST_ASSERT_EQUAL_UINT16(320, info.x_max);
    TEST_ASSERT_EQUAL_UINT16(240, info.y_max);
    TEST_ASSERT_TRUE(info.operation_mode == hal::DisplayTouchIface::OperationMode::Polling);

    hal::deinit_device(TestDisplayTouchDevice::NAME);
}

TEST_CASE("DisplayTouchIface: read_points", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayTouchDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayTouchIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    std::vector<hal::DisplayTouchIface::Point> points;
    TEST_ASSERT_TRUE(iface->read_points(points));
    TEST_ASSERT_EQUAL(2, points.size());
    TEST_ASSERT_EQUAL(12, points[0].x);
    TEST_ASSERT_EQUAL(34, points[0].y);
    TEST_ASSERT_EQUAL(56, points[0].pressure);
    TEST_ASSERT_EQUAL(78, points[1].x);
    TEST_ASSERT_EQUAL(90, points[1].y);
    TEST_ASSERT_EQUAL(12, points[1].pressure);

    hal::deinit_device(TestDisplayTouchDevice::NAME);
}

TEST_CASE("DisplayTouchIface: repeated init/deinit cycles work correctly", "[hal][device]")
{
    for (int i = 0; i < 3; i++) {
        BROOKESIA_LOGI("Cycle %1%", i + 1);

        TEST_ASSERT_TRUE(hal::init_device(TestDisplayTouchDevice::NAME));

        auto [name, iface] = hal::get_first_interface<hal::DisplayTouchIface>();
        TEST_ASSERT_NOT_NULL(iface.get());
        std::vector<hal::DisplayTouchIface::Point> points;
        TEST_ASSERT_TRUE(iface->read_points(points));

        hal::deinit_device(TestDisplayTouchDevice::NAME);

        auto [name2, iface2] = hal::get_first_interface<hal::DisplayTouchIface>();
        TEST_ASSERT_NULL(iface2.get());
    }
}


TEST_CASE("DisplayTouchIface: get_driver_specific", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestDisplayTouchDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::DisplayTouchIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::DisplayTouchIface::DriverSpecific specific;
    TEST_ASSERT_TRUE(iface->get_driver_specific(specific));
    TEST_ASSERT_NOT_NULL(specific.io_handle);
    TEST_ASSERT_NOT_NULL(specific.touch_handle);
}
