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

std::vector<hal::InterfaceSpec> TestDisplayPanelDevice::get_interface_specs() const
{
    return {{hal::display::PanelIface::NAME, TestDisplayPanelIface::NAME}};
}

bool TestDisplayPanelDevice::on_init()
{
    auto interface = std::make_shared<TestDisplayPanelIface>(hal::display::PanelIface::Info {
        .h_res = 320,
        .v_res = 240,
        .pixel_format = hal::display::PanelIface::PixelFormat::RGB565,
        .group_id = "display0",
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

TEST_CASE("display::PanelIface: acquire and draw", "[hal][interface]")
{
    auto handle = hal::acquire_first_interface<hal::display::PanelIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_EQUAL_STRING(TestDisplayPanelIface::NAME, std::string(handle.instance_name()).c_str());

    auto iface = handle.get();
    TEST_ASSERT_NOT_NULL(iface.get());
    const auto &info = iface->get_info();
    TEST_ASSERT_EQUAL_UINT32(320, info.h_res);
    TEST_ASSERT_EQUAL_UINT32(240, info.v_res);
    TEST_ASSERT_EQUAL(hal::display::PanelIface::PixelFormat::RGB565, info.pixel_format);
    TEST_ASSERT_EQUAL_STRING("display0", info.group_id.c_str());
    TEST_ASSERT_TRUE(info.is_valid());

    uint8_t data[] = {0xAA, 0x55};
    TEST_ASSERT_TRUE(iface->draw_bitmap(0, 0, 1, 1, data));
    TEST_ASSERT_FALSE(iface->draw_bitmap(0, 0, 0, 1, data));

    hal::display::PanelIface::DriverSpecific specific;
    TEST_ASSERT_TRUE(iface->get_driver_specific(specific));
}

TEST_CASE("display::PanelIface: acquire by instance and list handles", "[hal][interface]")
{
    auto handle = hal::acquire_interface<hal::display::PanelIface>(TestDisplayPanelIface::NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(handle));

    auto handles = hal::acquire_interfaces<hal::display::PanelIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, handles.size());
}
