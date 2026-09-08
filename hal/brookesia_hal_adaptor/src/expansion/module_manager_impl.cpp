/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_EXPANSION_MANAGER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include "private/utils.hpp"
#include "module_manager_impl.hpp"
#include "runtime.hpp"

namespace esp_brookesia::hal::expansion {

ModuleManagerImpl::~ModuleManagerImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::set<EventListenerId> listener_ids;
    {
        std::lock_guard lock(listener_mutex_);
        listener_ids.swap(listener_ids_);
    }
    for (const auto id : listener_ids) {
        detail::ExpansionRuntime::get_instance().remove_event_listener(id);
    }
}

std::vector<ModuleInfo> ModuleManagerImpl::get_module_infos() const
{
    return detail::ExpansionRuntime::get_instance().get_module_infos();
}

ModuleManagerIface::EventListenerId ModuleManagerImpl::add_event_listener(EventListener listener)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    const auto id = detail::ExpansionRuntime::get_instance().add_event_listener(std::move(listener));
    if (id != 0) {
        std::lock_guard lock(listener_mutex_);
        listener_ids_.insert(id);
    }
    return id;
}

bool ModuleManagerImpl::remove_event_listener(EventListenerId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    const bool removed = detail::ExpansionRuntime::get_instance().remove_event_listener(id);
    if (removed) {
        std::lock_guard lock(listener_mutex_);
        listener_ids_.erase(id);
    }
    return removed;
}

} // namespace esp_brookesia::hal::expansion
