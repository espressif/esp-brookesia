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
#include "brookesia/hal/device.hpp"

namespace esp_brookesia::hal {

bool Device::init_device_from_registry(void)
{
    auto agents = Device::Registry::get_all_instances();
    for (const auto &[_, plugin] : agents) {
        BROOKESIA_LOGI("Found Device plugin: %s", _);
        std::shared_ptr<Device> dev = Device::Registry::get_instance(_);
        if (dev == nullptr) {
            BROOKESIA_LOGE("- Get instance failed");
            return false;
        }
        if (!dev->probe()) {
            BROOKESIA_LOGE("- Probe failed");
            return false;
        }
        if (!dev->init()) {
            BROOKESIA_LOGE("- Init failed");
            return false;
        }
        BROOKESIA_LOGI("- Init success");
    };

    return true;
}

} // namespace esp_brookesia::hal
