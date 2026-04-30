/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <chrono>
#include <mutex>
#include "unity.h"
#include "boost/json.hpp"
#include "brookesia/hal_adaptor.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/service_manager.hpp"
#include "test_support.hpp"

namespace esp_brookesia::test_apps::service_device::board {

using DeviceHelper = service::helper::Device;
using NVSHelper = service::helper::NVS;

namespace {
auto &service_manager = service::ServiceManager::get_instance();
service::ServiceBinding device_binding;
service::ServiceBinding nvs_binding;
} // namespace

bool startup()
{
    hal::init_all_devices();
    if (!service_manager.start()) {
        return false;
    }

    nvs_binding = service_manager.bind(NVSHelper::get_name().data());
    if (!nvs_binding.is_valid()) {
        return false;
    }

    device_binding = service_manager.bind(DeviceHelper::get_name().data());
    return device_binding.is_valid();
}

void shutdown()
{
    device_binding.release();
    nvs_binding.release();
    service_manager.deinit();
    hal::deinit_all_devices();
}

DeviceHelper::Capabilities get_capabilities()
{
    auto result = DeviceHelper::call_function_sync<boost::json::object>(DeviceHelper::FunctionId::GetCapabilities);
    TEST_ASSERT_TRUE(result.has_value());

    DeviceHelper::Capabilities capabilities;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(result.value(), capabilities));
    return capabilities;
}

bool wait_for_number_event(EventState &event, double expected, uint32_t timeout_ms)
{
    std::unique_lock<std::mutex> lock(event.mutex);
    return event.cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&event, expected]() {
        return event.received && (event.number == expected);
    });
}

bool wait_for_boolean_event(EventState &event, bool expected, uint32_t timeout_ms)
{
    std::unique_lock<std::mutex> lock(event.mutex);
    return event.cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&event, expected]() {
        return event.received && (event.boolean == expected);
    });
}

bool wait_for_object_event(EventState &event, uint32_t timeout_ms)
{
    std::unique_lock<std::mutex> lock(event.mutex);
    return event.cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&event]() {
        return event.received;
    });
}

} // namespace esp_brookesia::test_apps::service_device::board
