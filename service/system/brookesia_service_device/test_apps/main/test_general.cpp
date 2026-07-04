/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "brookesia/lib_utils/test_adapter.hpp"
#include "boost/json.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_helper/system/device.hpp"
#include "test_support.hpp"

using namespace esp_brookesia;
using DeviceHelper = service::helper::Device;

BROOKESIA_TEST_CASE(test_servicedevice_board_general_board_query_smoke_test, "ServiceDevice board: general board query smoke test", "[service][device][board][general]")
{
    TEST_ASSERT_TRUE_MESSAGE(test_apps::service_device::board::startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        test_apps::service_device::board::shutdown();
    });

    auto capabilities = test_apps::service_device::board::get_capabilities();
    if (!test_apps::service_device::board::has_capability(capabilities, hal::system::BoardInfoIface::NAME)) {
        TEST_IGNORE_MESSAGE("Board info interface is not available on this board");
    }

    auto board_info_result = DeviceHelper::call_function_sync<boost::json::object>(DeviceHelper::FunctionId::GetBoardInfo);
    TEST_ASSERT_TRUE(board_info_result.has_value());

    DeviceHelper::BoardInfo info;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(board_info_result.value(), info));
    TEST_ASSERT_TRUE(info.is_valid());
}

BROOKESIA_TEST_CASE(
    test_servicedevice_board_general_camera_device_infos_smoke_test,
    "ServiceDevice board: general camera device infos smoke test", "[service][device][board][general]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(test_apps::service_device::board::startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        test_apps::service_device::board::shutdown();
    });

    auto capabilities = test_apps::service_device::board::get_capabilities();
    if (!test_apps::service_device::board::has_capability(capabilities, hal::video::CameraIface::NAME)) {
        TEST_IGNORE_MESSAGE("Camera interface is not available on this board");
        return;
    }

    auto infos_result = DeviceHelper::call_function_sync<boost::json::array>(
                            DeviceHelper::FunctionId::GetCameraDeviceInfos
                        );
    TEST_ASSERT_TRUE(infos_result.has_value());

    DeviceHelper::CameraDeviceInfos infos;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(infos_result.value(), infos));
}

BROOKESIA_TEST_CASE(
    test_servicedevice_board_general_network_connectivity_infos_smoke_test,
    "ServiceDevice board: general network connectivity infos smoke test", "[service][device][board][general]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(test_apps::service_device::board::startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        test_apps::service_device::board::shutdown();
    });

    auto capabilities = test_apps::service_device::board::get_capabilities();
    if (!test_apps::service_device::board::has_capability(capabilities, hal::network::ConnectivityIface::NAME)) {
        TEST_IGNORE_MESSAGE("Network connectivity interface is not available on this board");
        return;
    }

    auto infos_result = DeviceHelper::call_function_sync<boost::json::array>(
                            DeviceHelper::FunctionId::GetNetworkConnectivityInfo
                        );
    TEST_ASSERT_TRUE(infos_result.has_value());

    DeviceHelper::NetworkConnectivityInfos infos;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(infos_result.value(), infos));
    TEST_ASSERT_FALSE(infos.empty());
}
