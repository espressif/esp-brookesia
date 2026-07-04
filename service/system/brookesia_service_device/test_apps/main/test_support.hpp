/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <string_view>
#include "boost/json.hpp"
#include "brookesia/service_helper/system/device.hpp"

namespace esp_brookesia::test_apps::service_device::board {

bool startup();
bool bind_device();
void shutdown();

service::helper::Device::Capabilities get_capabilities();
bool has_capability(const service::helper::Device::Capabilities &capabilities, std::string_view interface_name);

} // namespace esp_brookesia::test_apps::service_device::board
