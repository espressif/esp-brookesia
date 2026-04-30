/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.hpp"
#include "test_general_board_info.hpp"

using namespace esp_brookesia;

namespace esp_brookesia {

bool TestBoardInfoDevice::probe()
{
    return true;
}

bool TestBoardInfoDevice::on_init()
{
    auto interface = std::make_shared<TestGeneralBoardInfoIface>();

    interfaces_.emplace(TestGeneralBoardInfoIface::NAME, interface);

    return true;
}

void TestBoardInfoDevice::on_deinit()
{
    interfaces_.clear();
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestBoardInfoDevice, std::string(TestBoardInfoDevice::NAME));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Test Cases //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("BoardInfoIface: get_device finds plugin after registration", "[hal][device]")
{
    auto device = hal::get_device_by_device_name(TestBoardInfoDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
}

TEST_CASE("BoardInfoIface: init_device registers interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestBoardInfoDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::BoardInfoIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    hal::deinit_device(TestBoardInfoDevice::NAME);
}

TEST_CASE("BoardInfoIface: deinit_device releases interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestBoardInfoDevice::NAME));

    hal::deinit_device(TestBoardInfoDevice::NAME);

    auto [name, iface] = hal::get_first_interface<hal::BoardInfoIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("BoardInfoIface: get_device returns correct device type", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestBoardInfoDevice::NAME));

    auto device = hal::get_device_by_device_name(TestBoardInfoDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
    TEST_ASSERT_EQUAL_STRING(TestBoardInfoDevice::NAME, std::string(device->get_name()).c_str());

    hal::deinit_device(TestBoardInfoDevice::NAME);
}

TEST_CASE("BoardInfoIface: get_first_interface returns BoardInfoIface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestBoardInfoDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::BoardInfoIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    TEST_ASSERT_EQUAL_STRING(TestGeneralBoardInfoIface::NAME, name.c_str());

    hal::deinit_device(TestBoardInfoDevice::NAME);
}

TEST_CASE("BoardInfoIface: get_first_interface returns empty pair when no devices initialized", "[hal][interface]")
{
    auto [name, iface] = hal::get_first_interface<hal::BoardInfoIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("BoardInfoIface: get_interfaces returns all BoardInfoIface instances", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestBoardInfoDevice::NAME));

    auto interfaces = hal::get_interfaces<hal::BoardInfoIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, interfaces.size());

    for (const auto &[iface_name, iface] : interfaces) {
        TEST_ASSERT_NOT_NULL(iface.get());
        BROOKESIA_LOGI("Found interface: %1%", iface_name);
    }

    hal::deinit_device(TestBoardInfoDevice::NAME);
}

TEST_CASE("BoardInfoIface: Device::get_interface returns interface from device instance", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestBoardInfoDevice::NAME));

    auto device = hal::get_device_by_device_name(TestBoardInfoDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::BoardInfoIface>(TestGeneralBoardInfoIface::NAME);
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::deinit_device(TestBoardInfoDevice::NAME);
}

TEST_CASE("BoardInfoIface: Device::get_interface returns nullptr for missing interface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestBoardInfoDevice::NAME));

    auto device = hal::get_device_by_device_name(TestBoardInfoDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::BoardInfoIface>("missing:BoardInfo");
    TEST_ASSERT_NULL(iface.get());

    hal::deinit_device(TestBoardInfoDevice::NAME);
}

TEST_CASE("BoardInfoIface: info is correctly propagated", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestBoardInfoDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::BoardInfoIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    const auto &info = iface->get_info();
    TEST_ASSERT_TRUE(info.is_valid());
    TEST_ASSERT_EQUAL_STRING("ESP-Test-Board", info.name.c_str());
    TEST_ASSERT_EQUAL_STRING(CONFIG_IDF_TARGET, info.chip.c_str());
    TEST_ASSERT_EQUAL_STRING("v1.2", info.version.c_str());
    TEST_ASSERT_EQUAL_STRING("Test board for HAL interface coverage", info.description.c_str());
    TEST_ASSERT_EQUAL_STRING("Espressif", info.manufacturer.c_str());

    hal::deinit_device(TestBoardInfoDevice::NAME);
}

TEST_CASE("BoardInfoIface: repeated init/deinit cycles work correctly", "[hal][device]")
{
    for (int i = 0; i < 3; i++) {
        BROOKESIA_LOGI("Cycle %1%", i + 1);

        TEST_ASSERT_TRUE(hal::init_device(TestBoardInfoDevice::NAME));

        auto [name, iface] = hal::get_first_interface<hal::BoardInfoIface>();
        TEST_ASSERT_NOT_NULL(iface.get());
        TEST_ASSERT_TRUE(iface->get_info().is_valid());

        hal::deinit_device(TestBoardInfoDevice::NAME);

        auto [name2, iface2] = hal::get_first_interface<hal::BoardInfoIface>();
        TEST_ASSERT_NULL(iface2.get());
    }
}
