/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_device/macro_configs.h"
#if !BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/expansion/module_manager.hpp"
#include "brookesia/service_device/service_device.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::service {

std::expected<boost::json::array, std::string> Device::function_get_expansion_module_infos()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!ensure_expansion_manager_iface()) {
        return std::unexpected("Expansion module manager interface is not available");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(expansion_.manager_iface->get_module_infos()).as_array();
}

bool Device::ensure_expansion_manager_iface()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (expansion_.manager_iface) {
        return true;
    }
    if (!hal::has_interface(hal::expansion::ModuleManagerIface::NAME)) {
        BROOKESIA_LOGD("Expansion module manager interface is not available");
        return false;
    }

    expansion_.manager_iface = hal::acquire_first_interface<hal::expansion::ModuleManagerIface>();
    return static_cast<bool>(expansion_.manager_iface);
}

void Device::request_expansion_module_events()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    expansion_.events_requested = true;
    if (!is_running()) {
        BROOKESIA_LOGD("Device service is not running yet, defer expansion module events");
        return;
    }
    if (!start_expansion_module_events()) {
        BROOKESIA_LOGW("Failed to start expansion module event forwarding after request");
    }
}

bool Device::start_expansion_module_events()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (expansion_.listener_id != 0) {
        return true;
    }
    BROOKESIA_CHECK_FALSE_RETURN(
        ensure_expansion_manager_iface(), false, "Failed to acquire expansion module manager interface"
    );

    expansion_.listener_id = expansion_.manager_iface->add_event_listener(
    [this](const hal::expansion::ModuleInfo & module) {
        if (!publish_expansion_module_changed(module)) {
            BROOKESIA_LOGW("Failed to publish expansion module changed event");
        }
    });
    BROOKESIA_CHECK_FALSE_RETURN(
        expansion_.listener_id != 0, false, "Failed to register expansion module event listener"
    );

    return true;
}

void Device::stop_expansion_module_events()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (expansion_.listener_id == 0) {
        return;
    }
    if (expansion_.manager_iface &&
            !expansion_.manager_iface->remove_event_listener(expansion_.listener_id)) {
        BROOKESIA_LOGW("Failed to remove expansion module event listener");
    }
    expansion_.listener_id = 0;
}

bool Device::publish_expansion_module_changed(const hal::expansion::ModuleInfo &module)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::ExpansionModuleChanged),
                      std::vector<EventItem> {EventItem(BROOKESIA_DESCRIBE_TO_JSON(module).as_object())}
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish expansion module changed event");

    return true;
}

} // namespace esp_brookesia::service
