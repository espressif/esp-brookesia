/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_interface/macro_configs.h"
#if !BROOKESIA_HAL_INTERFACE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

Device::~Device()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    deinit();
}

bool Device::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_initialized()) {
        BROOKESIA_LOGD("Already initialized");
        return true;
    }

    BROOKESIA_LOGD("- [%1%] Initializing...", name_);

    BROOKESIA_CHECK_FALSE_RETURN(on_init(), false, "[%1%] Initialization failed", name_);

    for (const auto &[name, interface] : interfaces_) {
        if (InterfaceRegistry::has_plugin(name)) {
            BROOKESIA_LOGW("- [%1%] Interface '%2%' already registered, skipping", name_, name);
            continue;
        }
        InterfaceRegistry::register_plugin<Interface>(name, [interface]() {
            return interface;
        });
        BROOKESIA_LOGI("- [%1%] Registered interface '%2%'", name_, name);
    }

    is_initialized_ = true;

    BROOKESIA_LOGI("- [%1%] Initialized", name_);

    return true;
}

void Device::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_initialized()) {
        BROOKESIA_LOGD("Not initialized, skip");
        return;
    }

    BROOKESIA_LOGD("- [%1%] Deinitializing...", name_);

    for (const auto &[name, interface] : interfaces_) {
        BROOKESIA_LOGI("- [%1%] Unregistering interface '%2%'", name_, name);
        InterfaceRegistry::remove_plugin(name);
    }

    on_deinit();

    interfaces_.clear();
    is_initialized_ = false;

    BROOKESIA_LOGI("- [%1%] Deinitialized", name_);
}

void init_all_devices()
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGI("Found %1% devices", DeviceRegistry::get_plugin_count());
    size_t count = 0;
    for (const auto &[name, _] : DeviceRegistry::get_all_instances()) {
        if (init_device(name)) {
            count++;
        }
    }
    BROOKESIA_LOGI("Initialized %1%/%2% devices", count, DeviceRegistry::get_plugin_count());
}

bool init_device(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    auto device = DeviceRegistry::get_instance(name);
    BROOKESIA_CHECK_NULL_RETURN(device, false, "- [%1%] Device not found", name);

    BROOKESIA_LOGD("- [%1%] Probing...", name);
    BROOKESIA_CHECK_FALSE_RETURN(device->probe(), false, "- [%1%] Probe failed, skipped", name);
    BROOKESIA_LOGD("- [%1%] Probed", name);

    BROOKESIA_CHECK_FALSE_RETURN(device->init(), false, "- [%1%] Initialization failed, skipped", name);

    return true;
}

void deinit_all_devices()
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Found %1% devices", DeviceRegistry::get_plugin_count());
    for (const auto &[name, _] : DeviceRegistry::get_all_instances()) {
        deinit_device(name);
    }
}

void deinit_device(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    auto device = DeviceRegistry::get_instance(name);
    BROOKESIA_CHECK_NULL_EXIT(device, "- [%1%] Device not found", name);

    device->deinit();
}

} // namespace esp_brookesia::hal
