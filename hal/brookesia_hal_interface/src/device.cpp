/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <list>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <array>
#include <memory>
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

bool Device::init_device_from_registry(void)
{
    auto devices = Device::Registry::get_all_instances();

    if (devices.empty()) {
        BROOKESIA_LOGE("No devices found in registry");
        return false;
    }

    for (const auto &[name, plugin] : devices) {
        BROOKESIA_LOGI("Found Device plugin: %1%", name);

        std::shared_ptr<Device> dev = Device::Registry::get_instance(name);
        BROOKESIA_CHECK_NULL_RETURN(dev, false, "Device plugin %1% not found", name);

        if (dev->probe()) {
            BROOKESIA_LOGI("Device %1% probe success", name);
            BROOKESIA_CHECK_FALSE_RETURN(dev->init(), false, "Device plugin %1% init failed", name);
        } else {
            BROOKESIA_LOGW("Device %1% probe fail");
        }
    }

    return true;
}

} // namespace esp_brookesia::hal
