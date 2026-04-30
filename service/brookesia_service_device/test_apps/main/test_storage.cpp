/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
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
}
