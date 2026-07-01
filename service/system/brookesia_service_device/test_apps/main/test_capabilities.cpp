/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_helper/system/device.hpp"
#include "test_support.hpp"

using namespace esp_brookesia;

BROOKESIA_TEST_CASE(
    test_servicedevice_board_capabilities_are_available,
    "ServiceDevice board: capabilities are available",
    "[service][device][board][capabilities]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(test_apps::service_device::board::startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        test_apps::service_device::board::shutdown();
    });

    auto capabilities = test_apps::service_device::board::get_capabilities();
    BROOKESIA_LOGI("Capabilities: %1%", BROOKESIA_DESCRIBE_TO_STR(capabilities));
}
