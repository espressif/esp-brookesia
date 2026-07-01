/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.hpp"
#include "test_storage_fs.hpp"

using namespace esp_brookesia;

namespace esp_brookesia {

bool TestStorageFsDevice::probe()
{
    return true;
}

std::vector<hal::InterfaceSpec> TestStorageFsDevice::get_interface_specs() const
{
    return {{hal::storage::FileSystemIface::NAME, TestStorageFsIface::NAME}};
}

bool TestStorageFsDevice::on_init()
{
    auto interface = std::make_shared<TestStorageFsIface>();

    interfaces_.emplace(TestStorageFsIface::NAME, interface);

    return true;
}

void TestStorageFsDevice::on_deinit()
{
    interfaces_.clear();
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestStorageFsDevice, std::string(TestStorageFsDevice::NAME));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Test Cases //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("storage::FileSystemIface: acquire returns file-system information", "[hal][interface]")
{
    auto handle = hal::acquire_first_interface<hal::storage::FileSystemIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_EQUAL_STRING(TestStorageFsIface::NAME, std::string(handle.instance_name()).c_str());

    auto iface = handle.get();
    TEST_ASSERT_NOT_NULL(iface.get());
    const auto infos = iface->get_all_info();
    TEST_ASSERT_EQUAL(2, infos.size());
    TEST_ASSERT_EQUAL_STRING("/spiffs", infos[0].root_path.c_str());
    TEST_ASSERT_EQUAL_STRING("/littlefs", infos[1].root_path.c_str());
    TEST_ASSERT_TRUE(infos[1].supports_directories);

    hal::storage::FileSystemIface::Capacity capacity;
    TEST_ASSERT_TRUE(iface->get_capacity("/littlefs", capacity));
    TEST_ASSERT_EQUAL_UINT32(2 * 1024 * 1024, capacity.total_bytes);
    TEST_ASSERT_FALSE(iface->get_capacity("/missing", capacity));
}

TEST_CASE("storage::FileSystemIface: acquire by instance and list handles", "[hal][interface]")
{
    auto handle = hal::acquire_interface<hal::storage::FileSystemIface>(TestStorageFsIface::NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(handle));

    auto handles = hal::acquire_interfaces<hal::storage::FileSystemIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, handles.size());
    TEST_ASSERT_TRUE(hal::has_interface<hal::storage::FileSystemIface>());
}
