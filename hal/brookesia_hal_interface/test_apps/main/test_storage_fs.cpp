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

TEST_CASE("StorageFsIface: get_device finds plugin after registration", "[hal][device]")
{
    auto device = hal::get_device_by_device_name(TestStorageFsDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
}

TEST_CASE("StorageFsIface: init_device registers interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestStorageFsDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::StorageFsIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    hal::deinit_device(TestStorageFsDevice::NAME);
}

TEST_CASE("StorageFsIface: deinit_device releases interfaces", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestStorageFsDevice::NAME));

    hal::deinit_device(TestStorageFsDevice::NAME);

    auto [name, iface] = hal::get_first_interface<hal::StorageFsIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("StorageFsIface: get_device returns correct device type", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestStorageFsDevice::NAME));

    auto device = hal::get_device_by_device_name(TestStorageFsDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
    TEST_ASSERT_EQUAL_STRING(TestStorageFsDevice::NAME, std::string(device->get_name()).c_str());

    hal::deinit_device(TestStorageFsDevice::NAME);
}

TEST_CASE("StorageFsIface: get_first_interface returns StorageFsIface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestStorageFsDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::StorageFsIface>();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(name.empty());

    TEST_ASSERT_EQUAL_STRING(TestStorageFsIface::NAME, name.c_str());

    hal::deinit_device(TestStorageFsDevice::NAME);
}

TEST_CASE("StorageFsIface: get_first_interface returns empty pair when no devices initialized", "[hal][interface]")
{
    auto [name, iface] = hal::get_first_interface<hal::StorageFsIface>();
    TEST_ASSERT_NULL(iface.get());
    TEST_ASSERT_TRUE(name.empty());
}

TEST_CASE("StorageFsIface: get_interfaces returns all StorageFsIface instances", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestStorageFsDevice::NAME));

    auto interfaces = hal::get_interfaces<hal::StorageFsIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, interfaces.size());

    for (const auto &[iface_name, iface] : interfaces) {
        TEST_ASSERT_NOT_NULL(iface.get());
        BROOKESIA_LOGI("Found interface: %1%", iface_name);
    }

    hal::deinit_device(TestStorageFsDevice::NAME);
}

TEST_CASE("StorageFsIface: Device::get_interface returns interface from device instance", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestStorageFsDevice::NAME));

    auto device = hal::get_device_by_device_name(TestStorageFsDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::StorageFsIface>(TestStorageFsIface::NAME);
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::deinit_device(TestStorageFsDevice::NAME);
}

TEST_CASE("StorageFsIface: Device::get_interface returns nullptr for missing interface", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestStorageFsDevice::NAME));

    auto device = hal::get_device_by_device_name(TestStorageFsDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    auto iface = device->get_interface<hal::StorageFsIface>("missing:StorageFs");
    TEST_ASSERT_NULL(iface.get());

    hal::deinit_device(TestStorageFsDevice::NAME);
}

TEST_CASE("StorageFsIface: get_all_info returns mounted file systems", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestStorageFsDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::StorageFsIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    const auto info = iface->get_all_info();
    TEST_ASSERT_EQUAL(2, info.size());
    TEST_ASSERT_EQUAL(hal::StorageFsIface::FileSystemType::SPIFFS, info[0].fs_type);
    TEST_ASSERT_EQUAL(hal::StorageFsIface::MediumType::Flash, info[0].medium_type);
    TEST_ASSERT_EQUAL_STRING("/spiffs", info[0].mount_point);
    TEST_ASSERT_EQUAL(hal::StorageFsIface::FileSystemType::LittleFS, info[1].fs_type);
    TEST_ASSERT_EQUAL(hal::StorageFsIface::MediumType::Flash, info[1].medium_type);
    TEST_ASSERT_EQUAL_STRING("/littlefs", info[1].mount_point);

    hal::deinit_device(TestStorageFsDevice::NAME);
}

TEST_CASE("StorageFsIface: repeated init/deinit cycles work correctly", "[hal][device]")
{
    for (int i = 0; i < 3; i++) {
        BROOKESIA_LOGI("Cycle %1%", i + 1);

        TEST_ASSERT_TRUE(hal::init_device(TestStorageFsDevice::NAME));

        auto [name, iface] = hal::get_first_interface<hal::StorageFsIface>();
        TEST_ASSERT_NOT_NULL(iface.get());
        TEST_ASSERT_EQUAL(2, iface->get_all_info().size());

        hal::deinit_device(TestStorageFsDevice::NAME);

        auto [name2, iface2] = hal::get_first_interface<hal::StorageFsIface>();
        TEST_ASSERT_NULL(iface2.get());
    }
}
