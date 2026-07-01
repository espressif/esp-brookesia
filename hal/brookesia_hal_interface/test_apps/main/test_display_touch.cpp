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

bool TestDisplayTouchIface::read_points(std::vector<hal::display::TouchIface::Point> &points)
{
    BROOKESIA_LOGI("Read points");
    points = {
        hal::display::TouchIface::Point{.x = 12, .y = 34, .pressure = 56, .track_id = 0},
        hal::display::TouchIface::Point{.x = 78, .y = 90, .pressure = 12, .track_id = 1},
    };
    return true;
}

bool TestDisplayTouchIface::register_interrupt_handler(hal::display::TouchIface::InterruptHandler handler, void *ctx)
{
    interrupt_handler_ = handler;
    interrupt_handler_ctx_ = ctx;
    return true;
}

bool TestDisplayTouchIface::get_driver_specific(hal::display::TouchIface::DriverSpecific &specific)
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

std::vector<hal::InterfaceSpec> TestDisplayTouchDevice::get_interface_specs() const
{
    return {{hal::display::TouchIface::NAME, TestDisplayTouchIface::NAME}};
}

bool TestDisplayTouchDevice::on_init()
{
    auto interface = std::make_shared<TestDisplayTouchIface>(hal::display::TouchIface::Info {
        .x_max = 320,
        .y_max = 240,
        .max_points = 5,
        .operation_mode = hal::display::TouchIface::OperationMode::Polling,
        .group_id = "display0",
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

TEST_CASE("display::TouchIface: acquire and read points", "[hal][interface]")
{
    auto handle = hal::acquire_first_interface<hal::display::TouchIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_EQUAL_STRING(TestDisplayTouchIface::NAME, std::string(handle.instance_name()).c_str());

    auto iface = handle.get();
    TEST_ASSERT_NOT_NULL(iface.get());
    const auto &info = iface->get_info();
    TEST_ASSERT_EQUAL_UINT32(320, info.x_max);
    TEST_ASSERT_EQUAL_UINT32(240, info.y_max);
    TEST_ASSERT_EQUAL_UINT32(5, info.max_points);
    TEST_ASSERT_EQUAL(hal::display::TouchIface::OperationMode::Polling, info.operation_mode);
    TEST_ASSERT_EQUAL_STRING("display0", info.group_id.c_str());
    TEST_ASSERT_TRUE(info.is_valid());

    std::vector<hal::display::TouchIface::Point> points;
    TEST_ASSERT_TRUE(iface->read_points(points));
    TEST_ASSERT_EQUAL(2, points.size());
    TEST_ASSERT_EQUAL_UINT32(12, points.front().x);

    bool interrupted = false;
    auto interrupt_handler = [](void *ctx) {
        auto *interrupted = static_cast<bool *>(ctx);
        *interrupted = true;
        return false;
    };
    TEST_ASSERT_TRUE(iface->register_interrupt_handler(interrupt_handler, &interrupted));
    (void)interrupted;

    hal::display::TouchIface::DriverSpecific specific;
    TEST_ASSERT_TRUE(iface->get_driver_specific(specific));
}

TEST_CASE("display::TouchIface: acquire by instance and list handles", "[hal][interface]")
{
    auto handle = hal::acquire_interface<hal::display::TouchIface>(TestDisplayTouchIface::NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(handle));

    auto handles = hal::acquire_interfaces<hal::display::TouchIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, handles.size());
}
