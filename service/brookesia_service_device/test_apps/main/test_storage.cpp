/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include <string>
#include "boost/json.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_helper/device.hpp"
#include "test_support.hpp"

using namespace esp_brookesia;
using DeviceHelper = service::helper::Device;

TEST_CASE("ServiceDevice board: storage query smoke test", "[service][device][board][storage]")
{
    TEST_ASSERT_TRUE_MESSAGE(test_apps::service_device::board::startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        test_apps::service_device::board::shutdown();
    });

    auto capabilities = test_apps::service_device::board::get_capabilities();
    if (capabilities.find(std::string(hal::StorageFsIface::NAME)) == capabilities.end()) {
        TEST_IGNORE_MESSAGE("Storage file-system interface is not available on this board");
    }

    auto storage_result = DeviceHelper::call_function_sync<boost::json::array>(
                              DeviceHelper::FunctionId::GetStorageFileSystems
                          );
    TEST_ASSERT_TRUE(storage_result.has_value());
    TEST_ASSERT_GREATER_THAN(0, storage_result.value().size());

    for (const auto &item : storage_result.value()) {
        TEST_ASSERT_TRUE(item.is_object());
        const auto &info = item.as_object();
        auto mount_point_it = info.find("mount_point");
        TEST_ASSERT_NOT_EQUAL(info.end(), mount_point_it);
        TEST_ASSERT_TRUE(mount_point_it->value().is_string());

        auto supports_directories_it = info.find("supports_directories");
        TEST_ASSERT_NOT_EQUAL(info.end(), supports_directories_it);
        TEST_ASSERT_TRUE(supports_directories_it->value().is_bool());

        std::string mount_point(mount_point_it->value().as_string().c_str());
        TEST_ASSERT_FALSE(mount_point.empty());

        auto capacity_result = DeviceHelper::call_function_sync<boost::json::object>(
                                   DeviceHelper::FunctionId::GetStorageFileSystemCapacity,
                                   mount_point
                               );
        TEST_ASSERT_TRUE(capacity_result.has_value());

        DeviceHelper::StorageFsCapacity capacity = {};
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(capacity_result.value(), capacity));
        TEST_ASSERT_GREATER_OR_EQUAL_UINT32(capacity.used_bytes, capacity.total_bytes);
        TEST_ASSERT_EQUAL_UINT32(capacity.total_bytes - capacity.used_bytes, capacity.free_bytes);
    }

    auto missing_result = DeviceHelper::call_function_sync<boost::json::object>(
                              DeviceHelper::FunctionId::GetStorageFileSystemCapacity,
                              std::string("/missing")
                          );
    TEST_ASSERT_FALSE(missing_result.has_value());
}
