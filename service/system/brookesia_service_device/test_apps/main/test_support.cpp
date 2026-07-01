/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "brookesia/lib_utils/test_adapter.hpp"
#include "boost/json.hpp"
#if defined(ESP_PLATFORM)
#include "brookesia/hal_adaptor.hpp"
#else
#include "brookesia/hal_linux.hpp"
#endif
#include "brookesia/hal_interface.hpp"
#include "brookesia/service_manager.hpp"
#include "test_support.hpp"

namespace esp_brookesia::test_apps::service_device::board {

using DeviceHelper = service::helper::Device;

namespace {
auto &service_manager = service::ServiceManager::get_instance();
service::ServiceBinding device_binding;
} // namespace

bool startup()
{
    if (!service_manager.start()) {
        return false;
    }
    return bind_device();
}

bool bind_device()
{
    if (device_binding.is_valid()) {
        return true;
    }

    device_binding = service_manager.bind(DeviceHelper::get_name().data());
    return device_binding.is_valid();
}

void shutdown()
{
    device_binding.release();
    service_manager.deinit();
}

DeviceHelper::Capabilities get_capabilities()
{
    auto result = DeviceHelper::call_function_sync<boost::json::array>(DeviceHelper::FunctionId::GetCapabilities);
    TEST_ASSERT_TRUE(result.has_value());

    DeviceHelper::Capabilities capabilities;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(result.value(), capabilities));
    return capabilities;
}

bool has_capability(const DeviceHelper::Capabilities &capabilities, std::string_view interface_name)
{
    for (const auto &device : capabilities) {
        for (const auto &interface : device.interfaces) {
            if (interface.type_name == interface_name) {
                return true;
            }
        }
    }
    return false;
}

} // namespace esp_brookesia::test_apps::service_device::board
