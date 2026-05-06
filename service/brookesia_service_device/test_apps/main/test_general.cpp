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

TEST_CASE("ServiceDevice board: general board query smoke test", "[service][device][board][general]")
{
    TEST_ASSERT_TRUE_MESSAGE(test_apps::service_device::board::startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        test_apps::service_device::board::shutdown();
    });

    auto capabilities = test_apps::service_device::board::get_capabilities();
    if (capabilities.find(std::string(hal::BoardInfoIface::NAME)) == capabilities.end()) {
        TEST_IGNORE_MESSAGE("Board info interface is not available on this board");
    }

    auto board_info_result = DeviceHelper::call_function_sync<boost::json::object>(DeviceHelper::FunctionId::GetBoardInfo);
    TEST_ASSERT_TRUE(board_info_result.has_value());

    DeviceHelper::BoardInfo info;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(board_info_result.value(), info));
    TEST_ASSERT_TRUE(info.is_valid());
}
