/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_EXPANSION_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include <exception>
#include <memory>

#include "private/utils.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/hal_adaptor/expansion/device.hpp"
#include "module_manager_impl.hpp"
#include "runtime.hpp"

namespace esp_brookesia::hal::expansion {

bool ExpansionDevice::probe()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    return detail::ExpansionRuntime::get_instance().has_providers();
}

std::vector<InterfaceSpec> ExpansionDevice::get_interface_specs() const
{
    return {{ModuleManagerIface::NAME, get_module_manager_iface_name()}};
}

bool ExpansionDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::string error_message;
    BROOKESIA_CHECK_FALSE_RETURN(
        detail::ExpansionRuntime::get_instance().retain_consumer(&error_message), false,
        "Failed to start expansion runtime: %1%", error_message
    );

    std::shared_ptr<ModuleManagerImpl> iface;
    try {
        iface = std::make_shared<ModuleManagerImpl>();
    } catch (const std::exception &e) {
        BROOKESIA_LOGE("Failed to create expansion manager interface: %1%", e.what());
        detail::ExpansionRuntime::get_instance().release_consumer();
        return false;
    } catch (...) {
        BROOKESIA_LOGE("Failed to create expansion manager interface");
        detail::ExpansionRuntime::get_instance().release_consumer();
        return false;
    }

    interfaces_.emplace(get_module_manager_iface_name(), std::move(iface));
    BROOKESIA_LOGI("Expansion module manager initialized");
    return true;
}

void ExpansionDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.clear();
    detail::ExpansionRuntime::get_instance().release_consumer();
}

#if BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, ExpansionDevice, ExpansionDevice::DEVICE_NAME, ExpansionDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_EXPANSION_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal::expansion
