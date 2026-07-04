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

std::vector<hal::InterfaceSpec> TestBoardInfoDevice::get_interface_specs() const
{
    return {{hal::system::BoardInfoIface::NAME, TestGeneralBoardInfoIface::NAME}};
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

TEST_CASE("system::BoardInfoIface: acquire returns board information", "[hal][interface]")
{
    auto handle = hal::acquire_first_interface<hal::system::BoardInfoIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_EQUAL_STRING(TestGeneralBoardInfoIface::NAME, std::string(handle.instance_name()).c_str());

    auto iface = handle.get();
    TEST_ASSERT_NOT_NULL(iface.get());
    const auto &info = iface->get_info();
    TEST_ASSERT_TRUE(info.is_valid());
    TEST_ASSERT_EQUAL_STRING("ESP-Test-Board", info.name.c_str());
    TEST_ASSERT_EQUAL_STRING(CONFIG_IDF_TARGET, info.chip.c_str());
    TEST_ASSERT_EQUAL_STRING("v1.2", info.version.c_str());
    TEST_ASSERT_EQUAL_STRING("Test board for HAL interface coverage", info.description.c_str());
    TEST_ASSERT_EQUAL_STRING("Espressif", info.manufacturer.c_str());
}

TEST_CASE("system::BoardInfoIface: acquire by instance and list handles", "[hal][interface]")
{
    auto handle = hal::acquire_interface<hal::system::BoardInfoIface>(TestGeneralBoardInfoIface::NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(handle));

    auto handles = hal::acquire_interfaces<hal::system::BoardInfoIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, handles.size());
    TEST_ASSERT_TRUE(hal::has_interface<hal::system::BoardInfoIface>());
}
